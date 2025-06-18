#include <stdio.h>
#include "pv_bdev.h"




void app_main(void)
{

    printf("Hello world from PV application!\n");
    
    /* Run peripheral tests */
    pv_test_sdc();
}
