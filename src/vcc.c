/* Kohei Kajimoto */

#include <stdio.h>
#include <stdlib.h>
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
	unsigned char *buf, unsigned int buf_len, int *len)
{
	int i;
	int num;
	for (i = 0; i < cmd_len; i++) {
		ftdi_setflowctrl(ftdi, 0);
		ftdi_write_data(ftdi, &cmd[i], 1);
	}
	num = ftdi_read_data(ftdi, buf, 256);
	for (i = 0; i < num; i++) {
		if (i == 2 || ((i - 2) % 20 == 0)) {
			printf("\n");
		}
		printf("%02X ", buf[i]);
	}
	*len = num;
	printf("\n");
}

void read_version(struct ftdi_context *ftdi)
{
	int num;
	unsigned char buf[256];

	begin(ftdi);
	buf[0] = 'V';
	buf[1] = 'X';
	send_command(ftdi, buf, 2, buf, 256, &num);
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
	send_command(ftdi, buf, 2, buf, 256, &num);
	printf("Product %d\n\n", buf[3]);

	end(ftdi);
}

void read_list(struct ftdi_context *ftdi)
{
	int i;
	int num;
	unsigned char buf[4096];

	begin(ftdi);

	buf[0] = 'O';
	buf[1] = 'X';

	send_command(ftdi, buf, 2, buf, 4096, &num);

	int ntracks = (num - 2) / 20;
	for (i = 0; i < ntracks; ++i) {
		int b = 20 * i + 2;
		int no = buf[b + 1];
		int npoints = buf[b + 2] + (buf[b + 3] << 8)
			+ (buf[b + 4] << 16) + (buf[b + 5] << 24);
		int sy = buf[b + 6] + 2000;
		int sm = buf[b + 7];
		int sd = buf[b + 8];
		int sh = buf[b + 9];
		int si = buf[b + 10];
		int ss = buf[b + 11];
		int ey = buf[b + 13] + 2000;
		int em = buf[b + 14];
		int ed = buf[b + 15];
		int eh = buf[b + 16];
		int ei = buf[b + 17];
		int es = buf[b + 18];
		printf("Log %4d  %6d"
			"  %04d-%02d-%02d %02d:%02d:%02d" 
			"  %04d-%02d-%02d %02d:%02d:%02d" 
			"\n",
			no, npoints,
			sy, sm, sd, sh, si, ss,
			ey, em, ed, eh, ei, es);
	}

	end(ftdi);
}

int main(int argc, char *argv[])
{
	int ret;
	int i;
	struct ftdi_context *ftdi;
	unsigned int chipid;
	unsigned char buf[256];

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

	read_version(ftdi);
	read_product(ftdi);
	read_list(ftdi);

	ftdi_usb_close(ftdi);
	ftdi_free(ftdi);

	return 0;
}

