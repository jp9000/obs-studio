#include <inttypes.h>
#include <stdio.h>
#include <windows.h>
#include "get-graphics-offsets.h"

int main(int argc, char *argv[])
{
	struct d3d8_offsets d3d8 = {0};
	struct d3d9_offsets d3d9 = {0};
	struct dxgi_offsets dxgi = {0};
	struct dxgi_offsets2 dxgi2 = {0};

	WNDCLASSA wc = {0};
	wc.style = CS_OWNDC;
	wc.hInstance = GetModuleHandleA(NULL);
	wc.lpfnWndProc = (WNDPROC)DefWindowProcA;
	wc.lpszClassName = DUMMY_WNDCLASS;

	SetErrorMode(SEM_FAILCRITICALERRORS);

	if (!RegisterClassA(&wc)) {
		printf("failed to register '%s'\n", DUMMY_WNDCLASS);
		return -1;
	}

	get_d3d9_offsets(&d3d9);
	get_d3d8_offsets(&d3d8);
	get_dxgi_offsets(&dxgi, &dxgi2);

	printf("[d3d8]\n");
	printf("present=0x%" PRIx32 "\n", d3d8.present);
	printf("[d3d9]\n");
	printf("present=0x%" PRIx32 "\n", d3d9.present);
	printf("present_ex=0x%" PRIx32 "\n", d3d9.present_ex);
	printf("present_swap=0x%" PRIx32 "\n", d3d9.present_swap);
	printf("d3d9_clsoff=0x%" PRIx32 "\n", d3d9.d3d9_clsoff);
	printf("is_d3d9ex_clsoff=0x%" PRIx32 "\n", d3d9.is_d3d9ex_clsoff);
	printf("[dxgi]\n");
	printf("present=0x%" PRIx32 "\n", dxgi.present);
	printf("present1=0x%" PRIx32 "\n", dxgi.present1);
	printf("resize=0x%" PRIx32 "\n", dxgi.resize);
	printf("release=0x%" PRIx32 "\n", dxgi2.release);

	(void)argc;
	(void)argv;
	return 0;
}
