#include "comskip.h"

int main (int argc, char ** argv)
{
	comskip_decode_state state;
  
	comskip_decode_init(&state, argc, argv);
	comskip_decode_loop(&state);
	comskip_decode_finish(&state);
	// review result
}
