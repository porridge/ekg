#define MAX_ITEMS 100
#define DEFAULT_DELAY 100000

struct action_data {
    	int act;
        int value[MAX_ITEMS];
	int delay[MAX_ITEMS];
};

enum { ACT_BLINK_LEDS = 1, ACT_BEEPS_SPK = 2 };

int blink_leds(int *flag, int *delay);
int beeps_spk(int *tone, int *delay);
