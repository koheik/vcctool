<?php
# VCC to TCX Converter
# Kohei Kajimoto (koheik@gmail.com)
# June 28, 2012

$tmpdir = "/home/koheik/vcctmp/";
# $tmpdir = "./tmp/";

set_time_limit(120);

function hubeny($lat1, $lon1, $lat2, $lon2)
{
	$deginrad = pi() / 180.0;
	# WGS84
	$wgs84a = 6378137.000;
	$wgs84b = 6356752.314245;
	$wgs84e2 = 0.00669437999019758;
	$d = 0.0;
	if (abs($lat1 - $lat2) < 1 && abs($lon1 - $lon2) < 1)
	{
		$lat = ($lat1 - $lat2) * $deginrad;
		$lon = ($lon1 - $lon2) * $deginrad;
		$sn = sin(0.5*($lat1 + $lat2) * $deginrad); 
		$cn = cos(0.5*($lat1 + $lat2) * $deginrad);
		$w = sqrt(1.0 - $wgs84e2 * $sn * $sn);
		$m = $wgs84a * (1.0 - $wgs84e2) / $w / $w / $w;
		$n = $wgs84a / $w;
		$d = sqrt($lat*$lat*$m*$m + $lon*$lon*$n*$n*$cn*$cn);
	}
	return $d;
}

function convert($fin, $fout)
{
	$bhrb = false;
	$bcad = false;
	$balt = false;
	
	libxml_use_internal_errors(true);
	$vcc = simplexml_load_file($fin, "SimpleXMLElement", LIBXML_PARSEHUGE);
	if (count(libxml_get_errors()) != 0)
	{
//		print_r(libxml_get_errors());
//		return false;
	}

	$trks = $vcc->CapturedTrack->Trackpoints->Trackpoint;
	$n = count($trks);
	if ($n == 0)
		return false;
	
	$stime = strtotime($trks[0]["dateTime"]);
	$etime = strtotime($trks[$n - 1]["dateTime"]);
	$totaltime = sprintf("%f", $etime - $stime);
	$stmstr = gmstrftime("%Y-%m-%dT%H:%M:%SZ", $stime);

	$dom = new DomDocument("1.0");
	$dom->formatOutput = true;

	$tcd = $dom->appendChild($dom->createElement("TrainingCenterDatabase"));
	$tcd->appendChild($dom->createAttribute("xsi:schemaLocation"))->value = "http://www.garmin.com/xmlschemas/TrainingCenterDatabase/v2 http://www.garmin.com/xmlschemas/TrainingCenterDatabasev2.xsd";
	$tcd->appendChild($dom->createAttribute("xmlns:xsi"))->value = "http://www.w3.org/2001/XMLSchema-instance";
	$tcd->appendChild($dom->createAttribute("xmlns"))->value = "http://www.garmin.com/xmlschemas/TrainingCenterDatabase/v2";
	$tcd->appendChild($dom->createAttribute("xmlns:ns2"))->value = "http://www.garmin.com/xmlschemas/UserProfile/v2";
	$tcd->appendChild($dom->createAttribute("xmlns:ns3"))->value = "http://www.garmin.com/xmlschemas/ActivityExtension/v2";
	$tcd->appendChild($dom->createAttribute("xmlns:ns4"))->value = "http://www.garmin.com/xmlschemas/ProfileExtension/v1";
	$tcd->appendChild($dom->createAttribute("xmlns:ns5"))->value = "http://www.garmin.com/xmlschemas/ActivityGoals/v1";

	$acts = $tcd->appendChild($dom->createElement("Activities"));
	$act = $acts->appendChild($dom->createElement("Activity"));
	$act->appendChild($dom->createAttribute("Sport"))->value = "Other";

	$eid = $act->appendChild($dom->createElement("Id"));
	$eid->appendChild($dom->createTextNode($stmstr));
	$lap = $act->appendChild($dom->createElement("Lap"));
	$lap->appendChild($dom->createAttribute("StartTime"))->value = $stmstr;
	$lap->appendChild($dom->createElement("TotalTimeSeconds"))->appendChild($dom->createTextNode($totaltime));
	$lap_dist = $lap->appendChild($dom->createElement("DistanceMeters"));
	$lap_maxs = $lap->appendChild($dom->createElement("MaximumSpeed"));
	if ($bhrb)
	{
		$lap_avghrb = $lap->appendChild($dom->createElement("AverageHeartRateBpm"))->appendChild($dom->createElement("Value"));
		$lap_maxhrb = $lap->appendChild($dom->createElement("MaximumHeartRateBpm"))->appendChild($dom->createElement("Value"));
	}
	$lap->appendChild($dom->createElement("Calories"))->appendChild($dom->createTextNode("0"));
	$lap->appendChild($dom->createElement("Intensity"))->appendChild($dom->createTextNode("Active"));
	if ($bcad)
	{
		$lap_cad = $lap->appendChild($dom->createElement("Cadance"));
	}
	$lap->appendChild($dom->createElement("TriggerMethod"))->appendChild($dom->createTextNode("Manual"));

	$knotinmps = 1852.00 / 3600;
	$trk = $lap->appendChild($dom->createElement("Track"));
	$dist_sum = 0.0;
	$maxspeed = 0.0;
	$lat2 = 0.0;
	$lon2 = 0.0;
	$ts_sum = 0.0;
	$tm2 = $stime;
	$maxhrb = 0;
	$thrb_sum = 0.0;
	$maxcad = 0;
	$tcad_sum = 0.0;
    foreach ($trks as $tp)
	{
		$tm1 = strtotime($tp["dateTime"]);
		$tmstr = gmstrftime("%Y-%m-%dT%H:%M:%SZ", $tm1);
		$knots = floatval($tp["speed"]);
		$mps = $knots * $knotinmps;
		if ($mps > $maxspeed)
			$maxspeed = $mps;
		$lat1 = floatval($tp["latitude"]);
		$lon1 = floatval($tp["longitude"]);
		$dist_sum += hubeny($lat1, $lon1, $lat2, $lon2);
		$dt = $tm1 - $tm2;
		$ts_sum += $dt * $mps;
		$altvalue = floatval($tp["heading"]);
		$hrbvalue = intval($knots * 10);
		if ($hrbvalue > $maxhrb)
			$maxhrb = $hrbvalue;
		$thrb_sum += $dt * $hrbvalue;
		$cadvalue = intval($tp["heading"]);
		if ($cadvalue > $maxcad)
			$maxcad = $cadvalue;
		$tcad_sum += $dt * $cadvalue;

		$lat2 = $lat1;
		$lon2 = $lon1;
		$tm2 = $tm1;
		
		$ntp = $trk->appendChild($dom->createElement("Trackpoint"));
		
		# time
		$ntm = $ntp->appendChild($dom->createElement("Time"));
		$ntm->appendChild($dom->createTextNode($tmstr));
		
		# position
		$nps = $ntp->appendChild($dom->createElement("Position"));
		# latitude
		$lat = $nps->appendChild($dom->createElement("LatitudeDegrees"));
		$lat->appendChild($dom->createTextNode($tp["latitude"]));
		# longitude
		$lon = $nps->appendChild($dom->createElement("LongitudeDegrees"));
		$lon->appendChild($dom->createTextNode($tp["longitude"]));

		# altitude
		if ($balt) {
	        $alt = $ntp->appendChild($dom->createElement("AltitudeMeters"));
    	    $alt->appendChild($dom->createTextNode($altvalue));
        }
        # dist
        $dsm = $ntp->appendChild($dom->createElement("DistanceMeters"));
        $dsm->appendChild($dom->createTextNode($dist_sum));
        # hrb
        if ($bhrb)
        {
	        $hrb = $ntp->appendChild($dom->createElement("HeartRateBpm"));
    	    $hrbv = $hrb->appendChild($dom->createElement("Value"));
        	$hrbv->appendChild($dom->createTextNode($hrbvalue));
		}
        # cadence
        if ($bcad)
        {
        	$cad = $ntp->appendChild($dom->createElement("Cadence"));
        	$cad->appendChild($dom->createTextNode($cadvalue));
		}
		# extensions
        $ext = $ntp->appendChild($dom->createElement("Extensions"));
		$tpx = $ext->appendChild($dom->createElement("TPX"));
		$tpx->appendChild($dom->createAttribute("xmlns"))->value = "http://www.garmin.com/xmlschemas/ActivityExtension/v2";
		$tpx->appendChild($dom->createElement("Speed"))->appendChild($dom->createTextNode($mps));
	}
	$maxcad = intval($tcad_sum / ($etime - $stime));
	$avghrb = intval($thrb_sum / ($etime - $stime));
	$avgspd = $ts_sum / ($etime - $stime);
	$lap_dist->appendChild($dom->createTextNode($dist_sum));
	$lap_maxs->appendChild($dom->createTextNode($maxspeed));
	if ($bhrb)
	{
		$lap_avghrb->appendChild($dom->createTextNode($avghrb));
		$lap_maxhrb->appendChild($dom->createTextNode($maxhrb));
	}
	if ($bcad)
	{
		$lap_cad->appendChild($dom->createTextNode($maxcad));
	}
	// lap extensions
	$ext = $lap->appendChild($dom->createElement("Exntensions"));
	$lx = $ext->appendChild($dom->createElement("LX"));
	$lx->appendChild($dom->createAttribute("xmlns"))->value = "http://www.garmin.com/xmlschemas/ActivityExtension/v2";
	$lx->appendChild($dom->createElement("MaxBikeCadence"))->appendChild($dom->createTextNode($maxcad));
	$lx = $ext->appendChild($dom->createElement("LX"));
	$lx->appendChild($dom->createAttribute("xmlns"))->value = "http://www.garmin.com/xmlschemas/ActivityExtension/v2";
	$lx->appendChild($dom->createElement("AvgSpeed"))->appendChild($dom->createTextNode($avgspd));

    # activity creator
	$crt = $act->appendChild($dom->createElement("Creator"));
	$crt->appendChild($dom->createAttribute("xsi:type"))->value="Device_t";
	$crt->appendChild($dom->createElement("Name"))->appendChild($dom->createTextNode("Velocitek ProStart"));
	$crt->appendChild($dom->createElement("UnitId"))->appendChild($dom->createTextNode($vcc->CapturedTrack->DeviceInfo["ftdiSerialNumber"]));
	$crt->appendChild($dom->createElement("ProductID"))->appendChild($dom->createTextNode("1"));

	$ver = $crt->appendChild($dom->createElement("Version"));
	$ver->appendChild($dom->createElement("VersionMajor"))->appendChild($dom->createTextNode("1"));
	$ver->appendChild($dom->createElement("VersionMinor"))->appendChild($dom->createTextNode("0"));
	$ver->appendChild($dom->createElement("BuildMajor"))->appendChild($dom->createTextNode("0"));
	$ver->appendChild($dom->createElement("BuildMinor"))->appendChild($dom->createTextNode("0"));

	$dom->save($fout);
	return true;
}

function convert_files($files, $wdir)
{
	$zipname = "";
	if (count($files) == 0)
		return $zipname;

	$zipname = $wdir . ".zip";
	$zip = new ZipArchive();
	$zip->open($zipname, ZIPARCHIVE::CREATE);
	mkdir($wdir . "/converted");
	$converted = array();
	foreach ($files as $f)
	{
		$bname = basename($f, ".vcc");
		$nname = $wdir . "/converted/" . $bname . ".tcx";
		if (convert($f, $nname))
			array_push($converted, $nname);
		unlink($f);
	}
	foreach ($converted as $n)
	{
		$zip->addFile($n, "converted/" . basename($n));
	}
	$zip->close();
	return $zipname;
}


# if (true)
if (isset($_FILES['vccin']))
{
	$rname = $_FILES['vccin']['name'];
	$tname = $_FILES['vccin']['tmp_name'];
#	$rname = "tracks.zip";
#	$tname = "tracks";
	$bname = basename($rname, ".vcc");
	$zname = basename($rname, ".zip");
	$files = array();
	$wdir = "";
	if ($zname != $rname && file_exists($tname))
        {
		$zip = new ZipArchive();
		$zip->open($tname);
		$wdir = $tmpdir . basename($tname);
		mkdir($wdir);
		$zip->extractTo($wdir);
		$files += glob($wdir . "/*.vcc");
		$files += glob($wdir . "/*/*.vcc");
	}
	elseif ($bname != $rname && file_exists($tname))
	{
		$wdir = $tmpdir . basename($tname);
		mkdir($wdir);
		$nname = $wdir . "/" . $rname;
		copy($tname, $nname);
		array_push($files, $nname);
	}
	if (file_exists($tname))
		unlink($tname);
	$zipname = convert_files($files, $wdir);
	if ($zipname != null)
	{
#		header("X-Sendfile: " . $zipname);
		header("Content-type: application/octet-stream");
#		header('Content-Disposition: attachment; filename="converted.zip"');
		header('Content-Disposition: attachment; filename="'. basename($zipname) . '"');
		header("Content-Length: " . filesize($zipname));
		readfile($zipname);
	} else {
		print "<HTML>\n";
		print "<BODY>\n";
		print "<H1>Sorry, something went wrong and cannot deliver it.</H1>\n";
		print "</BODY>\n";
		print "</HTML>\n";
	}
	system("/bin/rm -rf " . $wdir);
	system("/bin/rm -rf " . $zipname);
} else {
	print "<HTML>\n";
	print "<BODY>\n";
	print "<form enctype='multipart/form-data' action='index.php' method='POST'>\n";
	print "VCC File / Zip Archive: <input name='vccin' type='file' />\n";
	print "<input type='submit' value='convert' />\n";
	print "</form>\n";
	print "</BODY>\n";
	print "</HTML>\n";
}
?>
