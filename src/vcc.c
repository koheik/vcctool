/* Kohei Kajimoto */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <float.h>
#include <string.h>
#include <ftdi.h>

void setup(struct ftdi_context *ftdi)
{
	ftdi_set_baudrate(ftdi, 115200);
}

void begin(struct ftdi_context *ftdi)
{
	ftdi_usb_reset(ftdi);
	ftdi_usb_purge_rx_buffer(ftdi);
	ftdi_usb_purge_tx_buffer(ftdi);

	ftdi_setrts(ftdi, 1);
}

void end(struct ftdi_context *ftdi)
{
	ftdi_setrts(ftdi, 0);
}

void send_command(struct ftdi_context *ftdi,
	unsigned char *cmd, unsigned int cmd_len,
	unsigned char *buf, unsigned int buf_len, int *len, int cr)
{
	int i;
	int idx = 0;
	int num;
	for (i = 0; i < cmd_len; i++) {
		printf("cmd[%d] %02X\n", i, cmd[i]);
		ftdi_setflowctrl(ftdi, 0);
		ftdi_write_data(ftdi, &cmd[i], 1);
	}
	while((num = ftdi_read_data(ftdi, buf, buf_len)) > 0) {
		for (i = 0; i < num; i++) {
			if (idx == 2 || ((idx - 2) % cr == 0)) {
				printf("\n");
			}
			printf("%02X ", buf[i]);
			idx++;
		}
	}
	*len = idx;
	printf("\n");
}

void read_version(struct ftdi_context *ftdi)
{
	int num;
	unsigned char buf[256];

	begin(ftdi);
	buf[0] = 'V';
	buf[1] = 'X';
	send_command(ftdi, buf, 2, buf, 256, &num, 32);
	printf("Version %d.%d.%d.%d\n\n", buf[3], buf[4], buf[5], buf[6]);
	end(ftdi);
}

void read_product(struct ftdi_context *ftdi)
{
	int i;
	int num;
	unsigned char buf[256];

	begin(ftdi);

	buf[0] = 'P';
	buf[1] = 'X';
	send_command(ftdi, buf, 2, buf, 256, &num, 32);
	printf("Product %d\n\n", buf[3]);

	end(ftdi);
}

typedef struct {
	unsigned int no;
	unsigned int npoints;
	struct tm start;
	struct tm end;
	unsigned char finger_print[14];
} track;

void read_list(struct ftdi_context *ftdi, track **tracks, int *ntracks)
{
	int i;
	int num;
	unsigned char buf[4096];
	int n;
	char fmt1[64];
	char fmt2[64];
	track *trks;

	begin(ftdi);

	buf[0] = 'O';
	buf[1] = 'X';

	send_command(ftdi, buf, 2, buf, 4096, &num, 20);

	*ntracks = n = (num - 2) / 20;
	*tracks = trks = (track *)malloc(sizeof(track) * n);
	for (i = 0; i < n; ++i) {
		int b = 20 * i + 2;
		trks[i].no = buf[b + 1];
		trks[i].npoints = buf[b + 2] + (buf[b + 3] << 8)
			+ (buf[b + 4] << 16) + (buf[b + 5] << 24);
		trks[i].start.tm_year = buf[b +  6] + 100;
		trks[i].start.tm_mon  = buf[b +  7] - 1;
		trks[i].start.tm_mday = buf[b +  8];
		trks[i].start.tm_hour = buf[b +  9];
		trks[i].start.tm_min  = buf[b + 10];
		trks[i].start.tm_sec  = buf[b + 11];
		trks[i].end.tm_year = buf[b + 13] + 100;
		trks[i].end.tm_mon  = buf[b + 14] - 1;
		trks[i].end.tm_mday = buf[b + 15];
		trks[i].end.tm_hour = buf[b + 16];
		trks[i].end.tm_min  = buf[b + 17];
		trks[i].end.tm_sec  = buf[b + 18];
		memcpy(trks[i].finger_print, &buf[b + 6], 14);

		strftime(fmt1, 64, "%F %T", &trks[i].start);
		strftime(fmt2, 64, "%F %T", &trks[i].end);
		printf("Log %4d  %6d  %s %s\n", trks[i].no, trks[i].npoints, fmt1, fmt2);
	}

	end(ftdi);
}

typedef struct {
	struct tm time_stamp;
	float latitude;
	float longitude;
	float speed;
	float heading;
} point;

float float_decode(unsigned char *d)
{
	unsigned int base = 0x80 + (0x7F & d[1]);
	base = (base << 8) + d[2];
	base = (base << 8) + d[3];
	float ret = (float) (base) * powf(2.0f, d[0] - 150.0f);
	if (0x80 & d[1]) {
		ret = - ret;
	}
	return ret;
}

void read_track_points(struct ftdi_context *ftdi, track* trk, int idx)
{
	int i;
	char fmt[64];
	char name[64];
	int num;
	int sz = 32 * trk->npoints + 2;
	unsigned char *buf = (unsigned char*)malloc(sz);
	point *points = (point*) malloc(sizeof(point) * trk->npoints);

	begin(ftdi);

	for (i = 0; i < 14; ++i) {
		printf("%02X ", trk->finger_print[i]);
	}
	printf("\n");
	buf[0] = 'T';
	buf[1] = 'X';
	memcpy(&buf[2], trk->finger_print, 14);
	send_command(ftdi, buf, 16, buf, sz, &num, 32);


	float minLatitude = FLT_MAX;
	float maxLatitude = - FLT_MAX;
	float minLongitude = FLT_MAX;
	float maxLongitude = - FLT_MAX;

	printf("num=%d\n", num);
	for (i = 0; i < trk->npoints; ++i) {
		int b = 32 * i + 2;
		point *pt = &points[i];
		pt->time_stamp.tm_year = buf[b    ] + 100;
		pt->time_stamp.tm_mon  = buf[b + 1] - 1;
		pt->time_stamp.tm_mday = buf[b + 2];
		pt->time_stamp.tm_hour = buf[b + 3];
		pt->time_stamp.tm_min  = buf[b + 4];
		pt->time_stamp.tm_sec  = buf[b + 5];
		pt->latitude  = float_decode(&buf[b + 7]);
		pt->longitude = float_decode(&buf[b + 11]);
		pt->speed     = float_decode(&buf[b + 15]);
		pt->heading   = float_decode(&buf[b + 19]);

		minLatitude  = fmin(minLatitude,  pt->latitude);
		maxLatitude  = fmax(maxLatitude,  pt->latitude);
		minLongitude = fmin(minLongitude, pt->longitude);
		maxLongitude = fmax(maxLongitude, pt->longitude);
	}
	free(buf);
	end(ftdi);


	time_t t = timegm(&trk->start);
	strftime(name, 64, "%y%m%d_%H%M%S.vcc", localtime(&t));
	FILE *fp = fopen(name, "w+");
	name[0] = 0xEF;
	name[1] = 0xBB;
	name[2] = 0xBF;
	fwrite(name, 3, 1, fp);

	strftime(name, 64, "%y%m%d_%H%M%S", localtime(&t));
	t = time(NULL);
	strftime(fmt, 64, "%FT%T.0000000%z", localtime(&t));
	fprintf(fp, "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n");
	fprintf(fp, "<VelocitekControlCenter"
		" xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\""
		" xmlns:xsd=\"http://www.w3.org/2001/XMLSchema\""
		" createdOn=\"%s\""
		" xmlns=\"http://www.velocitekspeed.com/VelocitekControlCenter\">\r\n",
			fmt);
	fprintf(fp, "  <CapturedTrack name=\"%s\" downloadedOn=\"%s\" numberTrkpts=\"%d\">\r\n", name, fmt, trk->npoints);
	fprintf(fp, "    <MinLatitude>%.15f</MinLatitude>\r\n",   minLatitude);
	fprintf(fp, "    <MaxLatitude>%.15f</MaxLatitude>\r\n",   maxLatitude);
	fprintf(fp, "    <MinLongitude>%.15f</MinLongitude>\r\n", minLongitude);
	fprintf(fp, "    <MaxLongitude>%.15f</MaxLongitude>\r\n", maxLongitude);
	fprintf(fp, "    <DeviceInfo ftdiSerialNumber=\"%s\" />\r\n", "VP005594");
	fprintf(fp, "    <Trackpoints>\r\n");
	for (i = 0; i < trk->npoints; ++i) {
		point *pt = &points[i];
		t = timegm(&pt->time_stamp);
		strftime(fmt, 64, "%FT%T%z", localtime(&t));
		fprintf(fp, "      <Trackpoint dateTime=\"%s\" heading=\"%.2f\" speed=\"%.3f\""
				" latitude=\"%.15f\" longitude=\"%.15f\" />\r\n",
				fmt, pt->heading, pt->speed, pt->latitude, pt->longitude);
	}
	fprintf(fp, "    </Trackpoints>\r\n");
	fprintf(fp, "  </CapturedTrack>\r\n");
	fprintf(fp, "</VelocitekControlCenter>\r\n");
	fclose(fp);
}

int main(int argc, char *argv[])
{
	int ret;
	int i;
	struct ftdi_context *ftdi;
	unsigned int chipid;
	unsigned char buf[256];
	track *tracks;
	int ntracks;


	ftdi = ftdi_new();
	ret = ftdi_usb_open(ftdi, 0x0403, 0xB70A);
	if (ret < -1) {
		fprintf(stderr, "unable to open ftdi device: %d (%s)\n",
				ret, ftdi_get_error_string(ftdi));
		return ret;
	}

	printf("type=%d\n", ftdi->type);
	ftdi_read_chipid(ftdi, &chipid);
	printf("chipid=%x\n", chipid);


	setup(ftdi);

	ftdi_setrts(ftdi, 1);
	while((ret = ftdi_read_data(ftdi, buf, 256)) > 0) {
		for(i = 0; i < ret; ++i)
			printf("%02X ", buf[i]);
	}
	ftdi_setrts(ftdi, 0);

	read_version(ftdi);
	read_product(ftdi);

	read_list(ftdi, &tracks, &ntracks);

	if (argc > 1) {
		i = atoi(argv[1]);
		printf("tracks[%d].no=%d\n", i, tracks[i].no);
		read_track_points(ftdi, &tracks[i], i);
	}
	free(tracks);

	ftdi_usb_close(ftdi);
	ftdi_free(ftdi);

	return 0;
}

