# GPX1.0 to GPX1.1 Converter
# Kohei Kajimoto (koheik@gmail.com)
# March 17, 2018

require 'time'
require 'nokogiri'
require 'builder'

def parse(fn)
	doc = Nokogiri::XML(File.read(fn))
	doc.remove_namespaces!
	t = doc.xpath("/gpx/time").text
	tracks = []
	doc.xpath("//trk").each do |trk|
		n = trk.xpath("./name").text
		d = trk.xpath("./desc").text
		s = []
		trk.xpath(".//trkpt").each do |trkpt|
			e = trkpt.xpath("./ele").text
			t = trkpt.xpath("./time").text
			tt = Time.parse(t)
			mil = tt.to_f - tt.to_i
			if (mil > 0.1) then
				next
			end
			s.push({
				:lat => trkpt["lat"],
				:lon => trkpt["lon"],
				:ele => e,
				:time => t
			})
		end
		tracks.push({:time => t, :name => n, :desc => d, :segments => s})
	end
	return tracks
end

def build(params)
	xml = Builder::XmlMarkup.new(:indent => 2)
	xml.instruct! :xml, :encoding => "UTF-8"
	xml.gpx(
		"creator" => "Garmin Connect",
		"version" => "1.1",
		"xsi:schemaLocation" => "http://www.topografix.com/GPX/1/1 http://www.topografix.com/GPX/11.xsd",
		"xmlns:ns3" => "http://www.garmin.com/xmlschemas/TrackPointExtension/v1",
		"xmlns" => "http://www.topografix.com/GPX/1/1",
		"xmlns:xsi" => "http://www.w3.org/2001/XMLSchema-instance",
		"xmlns:ns2" => "http://www.garmin.com/xmlschemas/GpxExtensions/v3") do |gpx|
		gpx.metadata do |meta|
			meta.link(:href => "connect.garmin.com") do |link|
				link.text "Garmin Connect"
			end
			meta.time params[:time]
		end

		gpx.trk do |trk|
			trk.name params[:name]
			trk.desc params[:desc]
			trk.type "wind_kite_surfing"
			params[:segments].each do |segment|
				trk.trkseg do |seg|
					seg.trkpt(:lat => segment[:lat], :lon => segment[:lon]) do |pt|
						pt.ele segment[:ele]
						pt.time segment[:time]
						pt.extensions do |ext|
							ext.ns3:TrackPointExtension
						end
					end
				end
			end
		end
	end
	return xml
end

fn = ARGV[0]
parse(fn).each do |track|
	n = track[:name]
	n.gsub!(/[#: \/]/, "_")
	xml = build(track)
	File.open(n + ".gpx", "w+") do |f|
		f.write(xml.target!)
	end
end
