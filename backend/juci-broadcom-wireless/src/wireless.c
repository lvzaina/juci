
#include "main.h"
#include "tools.h"

#define MAX_RADIO	4
#define MAX_VIF		8
#define MAX_CLIENT	128

typedef struct {
	const char *vif;
	const char *device;
	const char *ssid;
	const char *network;
	int noise;
} Wireless;

typedef struct {
	const char *name;
	const char *band;
	int frequency;
	const char *hwmodes[6];
	int channels[16];
	const char *pcid;
	int bwcaps[4];
	bool is_ac;
} Radio;

typedef struct {
	bool exists;
	char macaddr[24];
	char wdev[8];
	int snr;
} Sta;

static Radio radio[MAX_RADIO];
static Wireless wireless[MAX_VIF];
static Sta stas[MAX_CLIENT];

enum {
	RADIO_NAME,
	VIF_NAME,
	__WL_MAX,
};

static const struct blobmsg_policy wl_policy[__WL_MAX] = {
	[RADIO_NAME] = { .name = "radio", .type = BLOBMSG_TYPE_STRING },
	[VIF_NAME] = { .name = "vif", .type = BLOBMSG_TYPE_STRING },
};

static void load_wireless() {
	struct uci_element *e;
	const char *device = NULL;
	const char *network = NULL;
	const char *ssid = NULL;
	char *token;
	char wdev[16];
	int rno = 0;
	int wno = 0;
	int chn;
	int vif;
	int vif0 = 0;
	int vif1 = 0;

	memset(wireless, '\0', sizeof(wireless));
	memset(radio, '\0', sizeof(radio));

	struct uci_package *uci_wireless = 0; 
	
	if((uci_wireless = init_package("wireless"))) {
		uci_foreach_element(&uci_wireless->sections, e) {
			struct uci_section *s = uci_to_section(e);

			if (!strcmp(s->type, "wifi-iface")) {
				device = uci_lookup_option_string(uci_ctx, s, "device");
				network = uci_lookup_option_string(uci_ctx, s, "network");
				ssid = uci_lookup_option_string(uci_ctx, s, "ssid");
				if (device) {
					wireless[wno].device = device;
					usleep(10000);
					wireless[wno].noise = atoi(chrCmd("wlctl -i %s noise", wireless[wno].device));
					(network) ? (wireless[wno].network = network) : (wireless[wno].network = "");
					(ssid) ? (wireless[wno].ssid = ssid) : (wireless[wno].ssid = "");
					if (!strcmp(device, "wl0")) {
						vif = vif0;
						vif0++;
					} else {
						vif = vif1;
						vif1++;
					}
					if (vif > 0)
						sprintf(wdev, "%s.%d", device, vif);
					else
						strcpy(wdev, device);

					wireless[wno].vif = strdup(wdev);

					wno++;
				}
			} else if (!strcmp(s->type, "wifi-device")) {
				radio[rno].name = s->e.name;
				if(!(radio[rno].band = uci_lookup_option_string(uci_ctx, s, "band")))
					radio[rno].band = "b";
				radio[rno].frequency = !strcmp(radio[rno].band, "a") ? 5 : 2;
				usleep(10000);
				runCmd("wlctl -i %s band %s", radio[rno].name, radio[rno].band);
				usleep(10000);
				radio[rno].pcid = chrCmd("wlctl -i %s revinfo | awk 'FNR == 2 {print}' | cut -d'x' -f2", radio[rno].name);
				radio[rno].is_ac = false;
				if (radio[rno].pcid && atoi(chrCmd("db -q get hw.%s.is_ac", radio[rno].pcid)) == 1)
					radio[rno].is_ac = true;

				if(radio[rno].frequency == 2) {
					radio[rno].hwmodes[0] = "11b";
					radio[rno].hwmodes[1] = "11g";
					radio[rno].hwmodes[2] = "11bg";
					radio[rno].hwmodes[3] = "11n";
					radio[rno].bwcaps[0] = 20;
					radio[rno].bwcaps[1] = 40;
					radio[rno].bwcaps[2] = '\0';
				} else if (radio[rno].frequency == 5) {
					radio[rno].hwmodes[0] = "11a";
					radio[rno].hwmodes[1] = "11n";
					radio[rno].hwmodes[2] = '\0';
					radio[rno].hwmodes[3] = '\0';
					radio[rno].bwcaps[0] = 20;
					radio[rno].bwcaps[1] = 40;
					radio[rno].bwcaps[2] = 80;
					radio[rno].bwcaps[3] = '\0';
					if (radio[rno].is_ac)
						radio[rno].hwmodes[2] = "11ac";
				}

				chn = 0;
				usleep(10000);
				token = strtok(chrCmd("wlctl -i %s channels", radio[rno].name), " ");
				while (token != NULL)
				{
					radio[rno].channels[chn] = atoi(token);
					if (radio[rno].channels[chn] > 48)
						break;
					token = strtok (NULL, " ");
					chn++;
				}
				radio[rno].channels[chn] = '\0';

				rno++;
			}
		}
	}
}

static int wireless_radios(struct ubus_context *ctx, struct ubus_object *obj,
	struct ubus_request_data *req, const char *method,
	struct blob_attr *msg)
{
	void *t, *c;
	int i, j;

	blob_buf_init(&bb, 0);

	for (i = 0; i < MAX_RADIO; i++) {
		if (!radio[i].name)
			break;
		t = blobmsg_open_table(&bb, radio[i].name);
		blobmsg_add_string(&bb, "band", radio[i].band);
		blobmsg_add_u32(&bb, "frequency", radio[i].frequency);
		c = blobmsg_open_array(&bb, "hwmodes");
		for(j=0; radio[i].hwmodes[j]; j++) {
			blobmsg_add_string(&bb, "", radio[i].hwmodes[j]);
		}
		blobmsg_close_array(&bb, c);
		c = blobmsg_open_array(&bb, "bwcaps");
		for(j=0; radio[i].bwcaps[j]; j++) {
			blobmsg_add_u32(&bb, "", radio[i].bwcaps[j]);
		}
		blobmsg_close_array(&bb, c);
		c = blobmsg_open_array(&bb, "channels");
		for(j=0; radio[i].channels[j]; j++) {
			blobmsg_add_u32(&bb, "", radio[i].channels[j]);
		}
		blobmsg_close_array(&bb, c);
		blobmsg_close_table(&bb, t);
	}

	ubus_send_reply(ctx, req, bb.head);

	return 0;
}


static void update_wireless_clients()
{
	FILE *assoclist;
	char cmnd[64];
	char line[64];
	int i = 0;
	int j = 0;
	int rssi = 0;
	
	load_wireless(); 
	
	for (i = 0; wireless[i].device; i++) {
		if (wireless[i].noise > -60) {
			usleep(10000);
			wireless[i].noise = atoi(chrCmd("wlctl -i %s noise", wireless[i].device));
		}
		usleep(10000);
		sprintf(cmnd, "wlctl -i %s assoclist", wireless[i].vif);
		if ((assoclist = popen(cmnd, "r"))) {
			while(fgets(line, sizeof(line), assoclist) != NULL)
			{
				remove_newline(line);
				stas[j].exists = false;
				if (sscanf(line, "assoclist %s", stas[j].macaddr) == 1) {
					stas[j].exists = true;
					strcpy(stas[j].wdev, wireless[i].vif);
					usleep(10000);
					rssi = atoi(chrCmd("wlctl -i %s rssi %s", wireless[i].vif, stas[j].macaddr));
					stas[j].snr = rssi - wireless[i].noise;
					j++;
				}
			}
			pclose(assoclist);
		}
	}
}

static int wireless_clients(struct ubus_context *ctx, struct ubus_object *obj,
		  struct ubus_request_data *req, const char *method,
		  struct blob_attr *msg)
{
	void *t;
	char clientnum[10];
	int num = 1;
	int i;

	blob_buf_init(&bb, 0);
	
	update_wireless_clients(); 
	
	for (i = 0; i < MAX_CLIENT; i++) {
		if (!stas[i].exists)
			break;
		sprintf(clientnum, "client-%d", num);
		t = blobmsg_open_table(&bb, clientnum);
		//blobmsg_add_string(&bb, "hostname", stas[i].hostname);
		//blobmsg_add_string(&bb, "ipaddr", stas[i].ipaddr);
		blobmsg_add_string(&bb, "macaddr", stas[i].macaddr);
		//blobmsg_add_string(&bb, "network", stas[i].network);
		//blobmsg_add_string(&bb, "device", stas[i].device);
		//blobmsg_add_u8(&bb, "dhcp", stas[i].dhcp);
		//blobmsg_add_u8(&bb, "connected", stas[i].connected);
		//blobmsg_add_u8(&bb, "wireless", stas[i].wireless);
		blobmsg_add_string(&bb, "wdev", stas[i].wdev);
		//blobmsg_add_u32(&bb, "snr", stas[i].snr);
		blobmsg_close_table(&bb, t);
		num++;
	}
	
	ubus_send_reply(ctx, req, bb.head);

	return 0;
}

static int 
wireless_info(struct ubus_context *ctx, struct ubus_object *obj,
		  struct ubus_request_data *req, const char *method,
		  struct blob_attr *msg)
{
	void *item;
	char wpaKey[32]; 
	
	blob_buf_init(&bb, 0);
	
	//get_db_hw_value("authKey", &keys->auth);
	//get_db_hw_value("desKey", &keys->des);
	get_db_hw_value("wpaKey", wpaKey);
	
	item = blobmsg_open_table(&bb, "defaults"); 
	blobmsg_add_string(&bb, "wpa_key", wpaKey);
	blobmsg_close_table(&bb, item);

	ubus_send_reply(ctx, req, bb.head);

	return 0;
}

struct ubus_object *wl_get_ubus_object(){
	static struct ubus_method wl_object_methods[] = {
		UBUS_METHOD_NOARG("info", wireless_info),
		UBUS_METHOD_NOARG("radios", wireless_radios),
		UBUS_METHOD_NOARG("clients", wireless_clients)
	};

	static struct ubus_object_type wl_object_type =
		UBUS_OBJECT_TYPE("broadcom-wl-type", wl_object_methods);

	static struct ubus_object wl_object = {
		.name = "juci.broadcom.wireless",
		.type = &wl_object_type,
		.methods = wl_object_methods,
		.n_methods = ARRAY_SIZE(wl_object_methods),
	};
	
	return &wl_object; 
}