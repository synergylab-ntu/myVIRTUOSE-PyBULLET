#include <myINCLUDES.h>                 // baked in
#include "VirtuoseAPI.h"                // comes from Haption
#include <myVIRTUOSE_v3.h>              // baked in
#include <myVIRTUOSE_UDP.h>             // baked in
#include <myVIRTUOSE_LOGGING.h>         // baked in
#include <myBULLET_v3.h>

int main()
{
    // UDP class object
    myVIRTUOSE_UDP myUDP(27017, 27018, "127.0.0.1", "127.0.0.1");
    float input_pos[7] = { 0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,1.0f };
    myUDP.setup_UDP();

    // Virtuose object attributes
    const char* myPORT = "127.0.0.1#53210";
    float myFORCEFACTOR = 1.0f, mySPEEDFACTOR = 1.0f, myDT = 0.01f;

    // Virtuose object definition
    float my_k = 10;
    ARM RightARM("127.0.0.1#53210", myFORCEFACTOR, mySPEEDFACTOR, myDT, my_k);
    RightARM.name = "RightARM";
    RightARM.quick_start(); // always needs to be done
    RightARM.debug_getPOS(); // always needs to be done
    

    // Impedance control parameters
    
    float my_b = 0.5;
    myBULLET SIM(myDT, my_b);

    // to be sent to Haption at the end - may consider making this part of the CMD structure?
    
    printf("Press Q to exit loop\n");
    while (!(GetKeyState('Q') & 0x8000)) {

        // Query Haption state and output through UDP
        myUDP.UDP_send_recv_v3(RightARM.getPOS()); // getPOS() computes frame H
        
        // Compute object frame O
        SIM.getSIM_state();

        // Computer f_i_plus
        RightARM.compute_f_cmd(SIM.O);

        // Apply forces in simulation
        SIM.apply_control_forces(RightARM.cmd.f_i_plus,RightARM.cmd.e_i);

        // Apply forces on handle
        RightARM.render_W_cmd();
     
        // Simulation step
        SIM.api.stepSimulation();
        Sleep(myDT);
    }

    // Haption clean up

    myUDP.cleanup();
    RightARM.quick_stop();

    return 0;
}