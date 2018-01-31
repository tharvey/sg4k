#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/serial.h>
#include <linux/types.h>
#include <sys/types.h>
#include <netinet/in.h>

#define u8 unsigned char
#define u16 unsigned short
#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x) (sizeof(x)/sizeof((x)[0]))
#endif

#define RESPONSE_TIMEOUT	1 /* seconds before response timeout */

struct __attribute__((__packed__)) header {
	u8 dir; /* AA from PC AB from device */
	u16 dev_id; /* 0x0000 */
	u8 len_lsb;
	u8 len_msb;
	u8 grp_addr;
	u8 dev_addr;
	u8 cmd_lsb;
	u8 cmd_msb;
};

struct __attribute__((__packed__)) packet {
	struct header h;
	u8 *data;
};

enum command {
	timing = 0x61,
	pattern = 0x62,
	colorspace = 0x63,
	deepcolor = 0x64,
	hdcp = 0x65,
	output_type = 0x66,

	audio_sampling = 0x67,
	audio_width = 0x68,
	audio_source= 0x69,
	audio_channel = 0x6a,

	user_timing = 0xa0,
	sink_edid = 0xaa,
	output_power = 0xab,

	set_addr = 0x7801,
	reset = 0x7802,

	read_native_timing = 0x80a1,
	read_output_status = 0x80a9,
	read_edid = 0xb838,
	read_hpd_status = 0xb839,
	read_address = 0xf801,

	response = 0xffff,
};

const char *timings[] = {
	"VESA640x480P_60HZ",
	"VESA800x600P_60HZ",
	"VESA1024x768P_60HZ",
	"VESA1280x768P_60HZ",
	"VESA1360x768P_60HZ",
	"VESA1280x960P_60HZ",
	"VESA1280x1024P_60HZ",
	"VESA1400x1050P_60HZ",
	"VESA1600x1200P_60HZ",
	"VESA1920x1200P_60HZ",
	"CEAVIC1440x480I_60HZ",
	"CEAVIC720x480P_60HZ",
	"CEAVIC1280x720P_60HZ",
	"CEAVIC1280x720P_59.94",
	"CEAVIC1920x1080I_60HZ",
	"CEAVIC1920x1080I_59.95HZ",
	"CEAVIC1920x1080P_30HZ",
	"CEAVIC1920x1080P_29.95HZ",
	"CEAVIC1920x1080P_24HZ",
	"CEAVIC1920x1080P_23.976HZ",
	"CEAVIC1920x1080P_60HZ",
	"CEAVIC1920x1080P_59.94HZ",
	"CEAVIC1440x576I_50HZ",
	"CEAVIC720x576P_50HZ",
	"CEAVIC1280x720P_50HZ",
	"CEAVIC1920x1080I_50HZ",
	"CEAVIC1920x1080P_25HZ",
	"CEAVIC1920x1080P_50HZ",
	"HDMIVIC4Kx2K_30HZ",
	"HDMIVIC4Kx2K_29.97HZ",
	"HDMIVIC4Kx2K_25HZ",
	"HDMIVIC4Kx2K_24HZ",
	"HDMIVIC4Kx2K_23.98HZ",
	"SMPTE4Kx2K_24HZ",
	"H20_4KYUV420_60HZ",
	"H20_4KYUV420_59.94HZ",
	"H20_4KYUV420_50HZ",
	"FP3D_1280x720P_60HZ",
	"FP3D_1280x720P_59.94HZ"
	"FP3D_1920x1080P_24HZ",
	"FP3D_1920x1080P_23.976HZ",
	"FP3D_1920x1080P_50HZ",
	"SBSHALF3D_1280x720P_59HZ",
	"SBSHALF3D_1920x1080I_59.94HZ",
	"SBSHALF3D_1920x1080P_59.94HZ",
	"SBSHALF3D_1920x1080P_23.976HZ",
	"SBSHALF3D_1280x720P_50HZ",
	"SBSHALF3D_1920x1080I_50HZ",
	"SBSHALF3D_1920x1080P_50HZ",
	"TAB3D_1280x720P_59.94HZ",
	"TAB3D_1920x1080P_59.94HZ",
	"TAB3D_1920x1080P_23.976HZ",
	"TAB3D_1280x1080P_23.976HZ",
	"TAB3D_1280x720P_50HZ",
	"TAB3D_1920x1080P_50HZ",
	"Auto",
	"User1",
	"User2",
	"User3",
	"User4",
	"User5",
	"User6",
	"User7",
	"User8",
	"User9",
	"User10",
};

enum response {
	response_ok = 0,
	response_crc_err,
	response_invalid_cmd,
	response_failed,
	response_invalid_param,
};

const char *responses[] = {
	"ok",
	"crc_err",
	"invalid_cmd",
	"failed",
	"invalid_param",
};

const char *patterns[] = {
	"100% ColorBar",
	"75% ColorBar",
	"8 StepGrayBar",
	"RedScreen",
	"GreenScreen",
	"BlueScreen",
	"YellowScreen",
	"CyanScreen",
	"MagentaScreen",
	"16 StepGrayBar",
	"WhiteScreen",
	"RGB Ramp",
	"Cross Black",
	"Cross Red",
	"Cross Green",
	"Cross Blue",
	"Square",
	"White dots",
	"AlternateWB",
	"White HScroll",
	"White VScroll",
	"Multiburst",
	"Ver-split",
	"Hor-split",
	"Red Ramp",
	"Green Ramp",
	"Blue Ramp",
	"W/B Bounce",
	"Border lines",
	"Window",
	"Target Circle",
	"Moving Ball",
	"3D boxes",
	"SMPTE ColorBar",
};

const char *colorspaces[] = {
	"RGB444",
	"YUV444",
	"YUV422",
	"Auto",
	"YUV420",
};

const char *deepcolors[] = {
	"24bit",
	"30bit",
	"36bit",
	"48bit",
	"Auto",
};

const char *output_types[] = {
	"DVI",
	"HDMI",
	"Auto",
};

const char *audio_samplerates[] = {
	"32KHz",
	"44.1KHz",
	"48KHz",
	"88KHz",
	"96KHz",
	"176KHz",
	"192KHz",
	"Auto",
};

const char *audio_widths[] = {
	"16bit",
	"20bit",
	"24bit",
	"Auto",
};

const char *audio_channels[] = {
	"2ch",
	"3ch",
	"4ch",
	"5ch",
	"6ch",
	"7ch",
	"8ch",
	"Auto",
};

const char *enables[] = {
	"off",
	"on",
};

struct termios orig_ttystate;
int debug = 0;

#define DPRINTF if (debug) printf

static void hexdump(const unsigned char *buf, int size)
{
	int i = 0;
	char ascii[20];

	ascii[0] = 0;
	for (i = 0; i < size; i++) {
		if (0 == (i % 16)) {
			if (ascii[0]) {
				ascii[16] = 0;
				printf("  |%s|\n", ascii);
				ascii[0] = 0;
			}
			printf("%08x ", i);
		}
		if (0 == (i % 8))
			printf(" ");
		printf("%02x ", buf[i]);
		ascii[i % 16] = (buf[i] < ' ' || buf[i] > 127) ? '.' : buf[i];
		ascii[(i % 16)+1] = 0;
	}
	for (; i % 16; i++) {
		if (0 == (i % 8))
			printf(" ");
		printf("   ");
	}
	printf("  |%-16s|\n", ascii);
}

static int open_dev(const char* dev)
{
	struct termios ttystate;
	int fd;

	printf("opening %s\n", dev);
	fd = open(dev, O_RDWR | O_NONBLOCK | O_NOCTTY);
	if (fd < 0) {
		perror("open");
		return fd;
	}

	// get original ttystate
	tcgetattr(fd, &orig_ttystate);

	// create a sane TTY state (raw mode, no HW/SF flow control)
	memset(&ttystate, 0, sizeof(ttystate));

	// enable receiver and ignore modem status lines
	ttystate.c_cflag |= CREAD;
	ttystate.c_cflag |= CLOCAL;
	// 8N1
	ttystate.c_cflag &= ~CSIZE;
	ttystate.c_cflag |= CS8;
	ttystate.c_cflag &= ~PARODD;
	ttystate.c_cflag &= ~PARENB;
	ttystate.c_cflag &= ~CSTOPB;
	if (cfsetispeed(&ttystate, B115200))
		perror("cfsetispeed");
	if (cfsetospeed(&ttystate, B115200))
		perror("cfsetospeed");
	// set tty state
	printf("setting ttystate\n");
	if (tcsetattr(fd, TCSANOW, &ttystate))
		perror("tcsetattr");

	// flush in/out data
	tcflush(fd, TCIOFLUSH);

	return fd;
}

/** return 2s complement 8bit checksum of data up to size bytes
 */
static u8 crc8(const u8 *data, int size)
{
	int i;
	u8 crc = 0;

	for (i = 0; i < size; i++)
		crc += data[i];
	return (0x100 - crc) & 0xff;
}

static const char *cmd(u16 key)
{
	static char str[32];
	const char *cmd = NULL;

	switch(key) {
	case set_addr:
		return "set_addr";
	case reset:
		return "reset";
	case response:
		return "response";
	};

	switch(key & 0x7fff) {
	case timing:
		cmd="timing";
		break;
	case pattern:
		cmd="pattern";
		break;
	case colorspace:
		cmd="colorspace";
		break;
	case deepcolor:
		cmd="deepcolor";
		break;
	case hdcp:
		cmd="hdcp";
		break;
	case output_type:
		cmd="output_type";
		break;
	case audio_sampling:
		cmd="audio_sampling";
		break;
	case audio_width:
		cmd="audio_width";
		break;
	case audio_channel:
		cmd="audio_channel";
		break;
	case user_timing:
		cmd="user_defined_timing";
		break;
	case sink_edid:
		cmd="sink_edid";
		break;
	case output_power:
		cmd="output_power";
		break;
	default:
		cmd="unk";
		break;
	};
	sprintf(str, "%s %s", key & 0x8000 ? "read" : "set", cmd);

	return str;
}

static int parse_packet(const u8 *buf, int size)
{
	struct packet *p = (struct packet *) buf;
	const u8 *d = buf + 9;
	const char* dir;
	int len = p->h.len_msb << 8 | p->h.len_lsb;
	int key = p->h.cmd_msb << 8 | p->h.cmd_lsb;
	int crc;

	switch(p->h.dir) {
	case 0xaa: dir="PC->MCU"; break;
	case 0xab: dir="MCU->PC"; break;
	default: dir="unk"; break;
	}
	crc = crc8(buf, size - 1);

	hexdump(buf, size);
	printf("\t%s\n", dir);
	printf("\tdev_id=0x%04x\n", p->h.dev_id);
	printf("\tcommand=%s (0x%04x)\n", cmd(key), key);
	printf("\tlen=%d\n", len);
	printf("\taddr=0x%02x 0x%02x\n", p->h.grp_addr, p->h.dev_addr);
	printf("\tcrc:0x%02x (0x%02x) %s\n", crc, buf[size - 1],
		crc == buf[size - 1] ? "ok" : "failed");
	printf("\tdata: %d bytes\n", len - 5);

	switch(key) {
	case set_addr:
		printf("\tset_addr: group=%d device=%d\n", d[0], d[1]);
		break;
	case reset:
		break;
	case user_timing:
		printf("\tread user_timing: index=%d)\n", d[0]);
		break;
	case read_native_timing:
		printf("\tread native_timing:\n");
		/* TODO: table 22 */
		break;
	case read_output_status:
		printf("\tread output_status:\n");
		/* TODO: table 23 */
		break;
	case sink_edid:
		/* TODO */
		printf("\tread sink_edid: output_port=%d\n", d[0]);
		break;
	case read_hpd_status:
		printf("\tread hpd_status: %s (%d)\n",
			d[0] ? "high" : "low", d[0]);
		break;
	case read_address:
		printf("\tread address: group=%d device=%d\n", d[0], d[1]);
		break;
	case 0xffff: {
		u16 _cmd = d[1] >> 8 | d[0];
		const char *s = "unk";

		if (d[2] <= ARRAY_SIZE(responses));
			s = responses[d[2]];
		printf("\tresponse to %s (0x%04x): %s\n", cmd(_cmd), _cmd, s);
		return 0;
	}	break;
	}

	switch(key & 0x7fff) {
	case timing:
		printf("\ttiming: %s (%d)\n", timings[d[0]], d[0]);
		break;
	case pattern:
		printf("\tpattern: %s (%d)\n", patterns[d[0]], d[0]);
		break;
	case colorspace:
		printf("\tcolorspace: %s (%d)\n", colorspaces[d[0]], d[0]);
		break;
	case deepcolor:
		printf("\tdeepcolor: %s (%d)\n", deepcolors[d[0]], d[0]);
		break;
	case hdcp:
		printf("\thdcp: %s (%d)\n", enables[d[0]], d[0]);
		break;
	case output_type:
		printf("\toutput_type: %s (%d)\n", output_types[d[0]], d[0]);
		break;
	case audio_sampling:
		printf("\taudio_samplerate: %s (%d)\n",
			audio_samplerates[d[0]], d[0]);
		break;
	case audio_width:
		printf("\taudio_width: %s (%d)\n",
			audio_widths[d[0]], d[0]);
		break;
	case audio_source:
		printf("\taudio_external_source: %s (%d)\n",
			enables[d[0]], d[0]);
		break;
	case audio_channel:
		printf("\taudio_channels: %s (%d)\n",
			audio_channels[d[0]], d[0]);
		break;
	case user_timing:
		printf("\tset user_timing: index=%d)\n", d[0]);
		/* TODO Table 14 */
		break;
	case sink_edid:
		printf("\tsink_edid: index=%d)\n", d[0]);
		break;
	case output_power:
		printf("\toutput_power: %s (%d))\n",
			d[0] ? "standby" : "normal", d[0]);
		break;
	};
}

static int send_command(int fd, u16 key, u8 *data, int size)
{
	u8 buf[256];
	int sz, ret;

	DPRINTF("%s cmd=%s (0x%04x) size=%d\n", __func__, cmd(key), key, size);
	if (size + sizeof(struct header) > sizeof(buf))
		return -EINVAL;

	struct packet *p = (struct packet *) &buf;
	p->h.dir = 0xaa; /* AA from PC AB from device */
	p->h.dev_id = 0;
	p->h.cmd_msb = key >> 8;
	p->h.cmd_lsb = key & 0xff;
	p->h.len_msb = (size + 5) >> 8;
	p->h.len_lsb = (size + 5) & 0xff;
	p->h.grp_addr = 0;
	p->h.dev_addr = 0;
	memcpy(&p->data, data, size);
	sz = size + sizeof(struct header) + 1;
	buf[sz - 1] = crc8(buf, sz - 1);

	DPRINTF("sending %d bytes...\n", sz);
	parse_packet(buf, sz);
	ret = write(fd, buf, sz);
	if (ret != sz)
		perror("tx");

	return ret;
}

static int read_response(int fd) {
	u8 buf[256];
	struct packet *p = (struct packet *) &buf;
	int rz, len;
	time_t start = time(NULL);

	len = 0;
	while (time(NULL) - start < RESPONSE_TIMEOUT) {
		rz = read(fd, buf + len, sizeof(buf) - len);
		if (rz < 0) {
			perror("read failed");
			break;
		}
		if (rz == 0)
			continue;
		len += rz;
		if (len < 5)
			continue;
		if (len >= 5 + (p->h.len_msb << 8 | p->h.len_lsb)) {
			DPRINTF("got %d byte packet\n", len);
			parse_packet(buf, len);
			if (crc8(buf, (len - 1)) != buf[len - 1]) {
				printf("crc failed\n");
				return -1;
			}
			return rz;
		}
	}
	return 0;
}

static void usage(const char *cmd)
{
	int i;

	fprintf(stderr, "usage: %s [options]\n", cmd);
	fprintf(stderr, "\n");
	fprintf(stderr, "  --debug             - verbose debugging\n");
	fprintf(stderr, "  --monitor           - monitor for status changes\n");
	fprintf(stderr, "  --device,-d <dev>   - specificy serial device\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "Video options:\n");
	fprintf(stderr, "  --pattern,-p <n>    - change display pattern\n");
	fprintf(stderr, "  --timing,-t <n>     - change timing (resolution)\n");
	fprintf(stderr, "  --colorspace,-c <n> - change colorspace\n");
	fprintf(stderr, "  --edid,-d <edid>\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "Audio options:\n");
	fprintf(stderr, "  --samplerate,-s <n> - change frequency\n");
	fprintf(stderr, "  --width,-w <n>      - change width\n");
	fprintf(stderr, "  --channels <n>      - change channels\n");
	fprintf(stderr, "\n");

	fprintf(stderr, "Timings:\n");
	for (i = 0; i < ARRAY_SIZE(timings); i++) {
		printf("\t%02d: %s\n", i, timings[i]);
	}

	fprintf(stderr, "\nPatterns:\n");
	for (i = 0; i < ARRAY_SIZE(patterns); i++) {
		printf("\t% 2d: %s\n", i, patterns[i]);
	}

	fprintf(stderr, "\nColorspace:\n");
	for (i = 0; i < ARRAY_SIZE(colorspaces); i++) {
		printf("\t% 2d: %s\n", i, colorspaces[i]);
	}

	fprintf(stderr, "\nDeepColor:\n");
	for (i = 0; i < ARRAY_SIZE(deepcolors); i++) {
		printf("\t% 2d: %s\n", i, deepcolors[i]);
	}

	fprintf(stderr, "\nOutput:\n");
	for (i = 0; i < ARRAY_SIZE(output_types); i++) {
		printf("\t% 2d: %s\n", i, output_types[i]);
	}

	fprintf(stderr, "\nAudioSamplerate: (--samplerate <n>)\n");
	for (i = 0; i < ARRAY_SIZE(audio_samplerates); i++) {
		printf("\t% 2d: %s\n", i, audio_samplerates[i]);
	}

	fprintf(stderr, "\nAudioWidth: (--width <n>)\n");
	for (i = 0; i < ARRAY_SIZE(audio_widths); i++) {
		printf("\t% 2d: %s\n", i, audio_widths[i]);
	}

	fprintf(stderr, "\nAudioChannels: (--channels <n>)\n");
	for (i = 0; i < ARRAY_SIZE(audio_channels); i++) {
		printf("\t% 2d: %s\n", i, audio_channels[i]);
	}

	fprintf(stderr, "\nAudioSource: (--source <n>)\n");
	for (i = 0; i < ARRAY_SIZE(enables); i++) {
		printf("\t% 2d: %s\n", i, enables[i]);
	}
}

/** main function
 */
int main(int argc, char** argv)
{
	const char *dev = NULL;
	u16 cmd = 0;
	u8 data = 0;
	int fd;
	int c;

	static struct option long_opts[] = {
		{ "debug",	required_argument, 0, 0  },
		{ "device",	required_argument, 0, 'd' },
		{ "pattern",	required_argument, 0, 'p' },
		{ "timing",	required_argument, 0, 't' },
		{ "colorspace",	required_argument, 0, 'c' },
		{ "edid",	required_argument, 0, 'e' },
		{ "samplerate",	required_argument, 0, 's' },
		{ "width",	required_argument, 0, 'w' },
		{ "channels",	required_argument, 0, 0 },
		{ "source",	required_argument, 0, 0 },
		{ "monitor",	no_argument, 0, 'm' },
		{ "reset",	no_argument, 0, 'r' },
		{ },
	};

	/* parse cmdline opts */
	while (1) {
		int opt_idx = 0;

		c = getopt_long(argc, argv, "hd:p:t:c:e:s:w:mr",
				long_opts, &opt_idx);
		if (c == -1)
			break;
		switch (c) {
		case 0:
			if (strcmp(long_opts[opt_idx].name, "debug") == 0) {
				debug = atoi(optarg);
			}
			else if (strcmp(long_opts[opt_idx].name,
					"channels") == 0)
			{
				cmd = audio_channel;
				data = atoi(optarg);
				if (data > ARRAY_SIZE(audio_channels)) {
					fprintf(stderr, "invalid index: %d\n",
						data);
					return -2;
				}
				printf("audio_channel: %s (%d)\n",
				       audio_channels[data], data);
			}
			else if (strcmp(long_opts[opt_idx].name,
					"source") == 0)
			{
				cmd = audio_source;
				data = atoi(optarg);
				if (data > ARRAY_SIZE(enables)) {
					fprintf(stderr, "invalid index: %d\n",
						data);
					return -2;
				}
				printf("audio_source: %s (%d)\n",
				       enables[data], data);
			}
			break;

		case 'd':
			dev = optarg;
			break;

		case 'e':
			cmd = read_edid;
			data = atoi(optarg);
			break;

		case 'c':
			cmd = colorspace;
			data = atoi(optarg);
			if (data > ARRAY_SIZE(colorspaces)) {
				fprintf(stderr, "invalid index: %d\n", data);
				return -2;
			}
			printf("colorspace: %s (%d)\n", colorspaces[data],
			       data);
			break;

		case 'm':
			cmd = 0;
			break;

		case 'r':
			cmd = reset;
			break;

		case 'p':
			cmd = pattern;
			data = atoi(optarg);
			if (data > ARRAY_SIZE(patterns)) {
				fprintf(stderr, "invalid index: %d\n", data);
				return -2;
			}
			printf("pattern: %s (%d)\n", patterns[data],
			       data);
			break;

		case 's':
			cmd = audio_sampling;
			data = atoi(optarg);
			if (data > ARRAY_SIZE(audio_samplerates)) {
				fprintf(stderr, "invalid index: %d\n", data);
				return -2;
			}
			printf("samplerate: %s (%d)\n", audio_samplerates[data],
			       data);
			break;

		case 't':
			cmd = timing;
			data = atoi(optarg);
			if (data > ARRAY_SIZE(timings)) {
				fprintf(stderr, "invalid index: %d\n", data);
				return -2;
			}
			printf("timing: %s (%d)\n", timings[data],
			       data);
			break;

		case 'w':
			cmd = audio_width;
			data = atoi(optarg);
			if (data > ARRAY_SIZE(audio_widths)) {
				fprintf(stderr, "invalid index: %d\n", data);
				return -2;
			}
			printf("audio_width: %s (%d)\n", audio_widths[data],
			       data);
			break;

		default:
			usage(argv[0]);
			return -1;
			break;
		}
	}

	if (optind < argc) {
	}

	if (!dev) {
		usage(argv[0]);
		return -1;
	}

	/* open device */
	fd = open_dev(dev);
	if (fd < 0) {
		perror("open");
		exit(-1);
	}

	/* send command */
	if (cmd) {
		send_command(fd, cmd, &data, 1);
		/* wait for response */
		DPRINTF("waiting for response...\n");
		read_response(fd);
	} else {
		printf("monitoring status...\n");
		while(1) {
			read_response(fd);
		}
	}

	/* restore terminal state */
	DPRINTF("restoring tty\n");
	tcsetattr(fd, TCSANOW, &orig_ttystate);

	close(fd);

	return 0;
}
