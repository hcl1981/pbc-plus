/* Host-side proof of the Micropolis engine through the engine.h seam,
 * exercising the real tool layer (w_tool.c / w_con.c) and a running sim.
 *
 * Build (from vendor/micropolis/sim):
 *   gcc -std=gnu99 -w -fcommon -DHEADLESS -I../../../src -Iheaders \
 *       s_*.c w_tool.c w_con.c rand.c random.c \
 *       ../../../src/micropolis_glue.c ../../../src/engine_micropolis.c \
 *       ../../../tools/host_sim_test.c -lm -o /tmp/simtest && /tmp/simtest
 */
#include "engine.h"
#include <stdio.h>
static int nz(void){int n=0;for(int x=0;x<120;x++)for(int y=0;y<100;y++)if(engine_tile(x,y))n++;return n;}
int main(void){
    long f0;
    engine_init();
    f0 = engine_funds();
    printf("init:  tiles=%d  funds=%ld  year=%d\n", nz(), f0, engine_year());

    /* coal plant + two residential zones adjacent to it + a road */
    engine_apply_tool(TOOL_POWERPLANT,  22, 22);
    engine_apply_tool(TOOL_RESIDENTIAL, 26, 23);
    engine_apply_tool(TOOL_RESIDENTIAL, 30, 23);
    for (int x=22;x<34;x++) engine_apply_tool(TOOL_ROAD, x, 26);
    printf("built: funds=%ld  (spent %ld)  res@26,23 powered=%d\n",
           engine_funds(), f0-engine_funds(), engine_powered(26,23));

    for (int i=0;i<4000;i++) engine_tick();
    printf("4000t: year=%d  funds=%ld  res@26,23 powered=%d  R/C/I=%d/%d/%d\n",
           engine_year(), engine_funds(), engine_powered(26,23),
           engine_demand_r(), engine_demand_c(), engine_demand_i());
    return 0;
}
