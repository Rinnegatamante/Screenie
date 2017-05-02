#include <vitasdk.h>
#include <taihen.h>
#include <libk/string.h>
#include <libk/stdio.h>
#include <kuio.h>

#define HOOKS_NUM  1    // Hooked functions num
#define CHUNK_SIZE 2048 // Screenshot buffer size in bytes

static uint8_t current_hook = 0;
static SceUID hooks[HOOKS_NUM];
static tai_hook_ref_t refs[HOOKS_NUM];

static char fname[256];
static char titleid[16];
static SceCtrlData pad;
static uint8_t sshot_buffer[CHUNK_SIZE];

void hookFunction(uint32_t nid, const void *func){
	hooks[current_hook] = taiHookFunctionImport(&refs[current_hook], TAI_MAIN_MODULE, TAI_ANY_LIBRARY, nid, func);
	current_hook++;
}

int sceDisplaySetFrameBuf_patched(const SceDisplayFrameBuf *pParam, int sync) {
	sceCtrlPeekBufferPositive(0, &pad, 1);
	if ((pad.buttons & SCE_CTRL_SELECT) && (pad.buttons & SCE_CTRL_LTRIGGER) && (pad.buttons & SCE_CTRL_RTRIGGER)){
		
		// Opening screenshot output file
		SceDateTime time;
		sceRtcGetCurrentClockLocalTime(&time);
		sprintf(fname, "ux0:/data/Screenie/%s-%d-%d-%d-%d-%d-%d.bmp", titleid, time.year, time.month, time.day, time.hour, time.minute, time.second);
		SceUID fd;
		kuIoOpen(fname, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_APPEND, &fd);
		
		// Writing Bitmap Header
		memset(sshot_buffer, 0, 0x36);
		*((uint16_t*)(&sshot_buffer[0x00])) = 0x4D42;
		*((uint32_t*)(&sshot_buffer[0x02])) = ((pParam->width*pParam->height)<<2)+0x36;
		*((uint32_t*)(&sshot_buffer[0x0A])) = 0x36;
		*((uint32_t*)(&sshot_buffer[0x0E])) = 0x28;
		*((uint32_t*)(&sshot_buffer[0x12])) = pParam->width;
		*((uint32_t*)(&sshot_buffer[0x16])) = pParam->height;
		*((uint32_t*)(&sshot_buffer[0x1A])) = 0x00200001;
		*((uint32_t*)(&sshot_buffer[0x22])) = ((pParam->width*pParam->height)<<2);
		kuIoWrite(fd, sshot_buffer, 0x36);
		
		// Writing Bitmap Table
		int x, y, i;
		i = 0;
		uint32_t* buffer = (uint32_t*)sshot_buffer;
		uint32_t* framebuf = (uint32_t*)pParam->base;
		for (y = 1; y<=pParam->height; y++){
			for (x = 0; x<pParam->width; x++){
				buffer[i] = framebuf[x+(pParam->height-y)*pParam->pitch];
				uint8_t* clr = (uint8_t*)&buffer[i];
				uint8_t g = clr[1];
				uint8_t r = clr[2];
				uint8_t a = clr[3];
				uint8_t b = clr[0];
				buffer[i] = (a<<24) | (b<<16) | (g<<8) | r;
				i++;
				if (i == (CHUNK_SIZE>>2)){
					i = 0;
					kuIoWrite(fd, sshot_buffer, CHUNK_SIZE);
				}
			}
		}
		if (i != 0) kuIoWrite(fd, sshot_buffer, i<<2);
		
		// Saving file
		kuIoClose(fd);
	
	}
	
	return TAI_CONTINUE(int, refs[0], pParam, sync);
}

void _start() __attribute__ ((weak, alias ("module_start")));
int module_start(SceSize argc, const void *args) {
	
	// Getting game Title ID
	sceAppMgrAppParamGetString(0, 12, titleid , 256);
	
	kuIoMkdir("ux0:/data/Screenie"); // Just in case the folder doesn't exist
	
	hookFunction(0x7A410B64,sceDisplaySetFrameBuf_patched);
	
	return SCE_KERNEL_START_SUCCESS;
}

int module_stop(SceSize argc, const void *args) {

	// Freeing hooks
	while (current_hook-- > 0){
		taiHookRelease(hooks[current_hook], refs[current_hook]);
	}

	return SCE_KERNEL_STOP_SUCCESS;
	
}