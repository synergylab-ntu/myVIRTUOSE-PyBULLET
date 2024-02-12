
#include "SharedMemory/b3RobotSimulatorClientAPI_NoDirect.h"
#include "SharedMemory/PhysicsClientSharedMemory_C_API.h"
#include "SharedMemory/b3RobotSimulatorClientAPI_InternalData.h"

#include <vector>
#include <Windows.h>
#include <iostream>

#include <myINCLUDES.h>         // baked in
#include "VirtuoseAPI.h"
#include <myVIRTUOSE_v2.h>         // baked in
#include <myVIRTUOSE_UDP.h>     // baked in
#include <myVIRTUOSE_LOGGING.h> // baked in

class myBULLET {
public:
    float time_step;
    b3RobotSimulatorClientAPI_NoDirect api;
    b3PhysicsClientHandle client;
    b3RobotSimulatorClientAPI_InternalData data;
    b3RobotSimulatorChangeDynamicsArgs dyn_args;
    b3RobotSimulatorGetContactPointsArgs myC;
    b3ContactInformation contactInfo;
    int world, actor;
    float force[3], torque[3];
    float f_prime[3], t_prime[3];
    float v1[3], v2[3];
    float cross_v1v2[3];
    float positiononA[3], positiononB[3];
    int N_con;
    float R_actor[3][3];
    float RT_actor[3][3];
    float minus_RT_actor[3][3];
    btVector3 pos_actor, pdot_actor, omega_actor;
    btQuaternion quat_actor;
    float norm_quat;
    float Ax[3];
    float AB[3][3];
    float p_cross[3][3];

    float k,b;
    //float K[3][3];
    float p_cmd[3];
    float f_cmd[3];
    

    //float quat_actor[4];
    //float pos_actor[3];

    void init() {
        b3PhysicsClientHandle client = b3ConnectSharedMemory(SHARED_MEMORY_KEY);
        if (!b3CanSubmitCommand(client))
        {
            printf("Not connected, start a PyBullet server first, using python -m pybullet_utils.runServer\n");
            exit(0);
        }

        data.m_physicsClientHandle = client;
        data.m_guiHelper = 0;
        
        api.setInternalData(&data);
        api.setTimeStep(time_step);
        api.resetSimulation();
        api.setGravity(btVector3(btScalar(0), btScalar(0), btScalar(-9.8)));
        world = api.loadURDF("plane.urdf");
        actor = api.loadURDF("sphere2.urdf");

        //btQuaternion

        api.resetBasePositionAndOrientation(actor, btVector3(btScalar(0), btScalar(0), btScalar(1)), btQuaternion(btScalar(0), btScalar(0), btScalar(0), btScalar(1)));
        dyn_args.m_mass = 0.1;

        api.changeDynamics(actor, -1, dyn_args);

        myC.m_bodyUniqueIdA = actor; // VERY IMPORTANT to have the actor as bodyA.
        myC.m_bodyUniqueIdB = world;
        myC.m_linkIndexA = -1;
        myC.m_linkIndexB = -1;

        k = 8;
        b = 0.5;
    }

    void myCROSS(float v1_in[3], float v2_in[3]) {
        for (int n = 0; n < 3; n++) {
            v1[n] = v1_in[n];
            v2[n] = v2_in[n];
        }
        
        cross_v1v2[0] = v1[1] * v2[2] - v2[1] * v1[2];
        cross_v1v2[1] = v2[0] * v1[2] - v1[0] * v2[2];
        cross_v1v2[2] = v1[0] * v2[1] - v2[0] * v1[1];
        std::cout << "Cross product result is : " << cross_v1v2[0] << "," << cross_v1v2[1] << "," << cross_v1v2[2] << std::endl;
    }

    void multiplyAx(float A[3][3], float x[3]) {
        Ax[0] = A[0][0] * x[0] + A[0][1] * x[1] + A[0][2] * x[2];
        Ax[1] = A[1][0] * x[0] + A[1][1] * x[1] + A[1][2] * x[2];
        Ax[2] = A[2][0] * x[0] + A[2][1] * x[1] + A[2][2] * x[2];
    }

    void multiplyAB(float A[3][3], float B[3][3]) {
        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < 3; j++) {
                AB[i][j] = 0;
            }
        }
        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < 3; j++) {
                AB[i][j] = AB[i][j] + A[i][j] * B[j][i];
            }
        }
    }

    void getSE3() {
        api.getBasePositionAndOrientation(actor, pos_actor, quat_actor);
        api.getBaseVelocity(actor, pdot_actor, omega_actor);
        //std::cout << "position - x = " << pos_actor[0] << ", y = " << pos_actor[1] << ", z = " << pos_actor[2] << std::endl;
        float x, y, z, w, a11, a12, a13, a21, a22, a23, a31, a32, a33;
        //std::cout << "quaternion - x = " << quat_actor.getX() << ", y = " << quat_actor.getY() << ", z = " << quat_actor.getZ() << ", w = " << quat_actor.getW() << std::endl;
        x = quat_actor.getX();
        y = quat_actor.getY();
        z = quat_actor.getZ();
        w = quat_actor.getW();


        norm_quat = x * x + y * y + z * z + w * w;
        if (norm_quat == 0)
        {
            R_actor[0][0] = 1;
            R_actor[0][1] = 0;
            R_actor[0][2] = 0;
            R_actor[1][0] = 0;
            R_actor[1][1] = 1;
            R_actor[1][2] = 0;
            R_actor[2][0] = 0;
            R_actor[2][1] = 0;
            R_actor[2][2] = 1;
        }
        else
        {
            a11 = (w * w + x * x + y * y - z * z) / norm_quat;
            a12 = 2 * (x * y - w * z) / norm_quat;
            a13 = 2 * (x * z + w * y) / norm_quat;
            a21 = 2 * (x * y + w * z) / norm_quat;
            a22 = (w * w - x * x + y * y - z * z) / norm_quat;
            a23 = 2 * (y * z - w * x) / norm_quat;
            a31 = 2 * (x * z - w * y) / norm_quat;
            a32 = 2 * (y * z + w * x) / norm_quat;
            a33 = (w * w - x * x - y * y + z * z) / norm_quat;
            R_actor[0][0] = a11;
            R_actor[0][1] = a12;
            R_actor[0][2] = a13;
            R_actor[1][0] = a21;
            R_actor[1][1] = a22;
            R_actor[1][2] = a23;
            R_actor[2][0] = a31;
            R_actor[2][1] = a32;
            R_actor[2][2] = a33;
        }

        RT_actor[0][0] = R_actor[0][0];
        RT_actor[0][1] = R_actor[1][0];
        RT_actor[0][2] = R_actor[2][0];
        RT_actor[1][0] = R_actor[0][1];
        RT_actor[1][1] = R_actor[1][1];
        RT_actor[1][2] = R_actor[2][1];
        RT_actor[2][0] = R_actor[0][2];
        RT_actor[2][1] = R_actor[1][2];
        RT_actor[2][2] = R_actor[2][2];

        minus_RT_actor[0][0] = -RT_actor[0][0];
        minus_RT_actor[0][1] = -RT_actor[1][0];
        minus_RT_actor[0][2] = -RT_actor[2][0];
        minus_RT_actor[1][0] = -RT_actor[0][1];
        minus_RT_actor[1][1] = -RT_actor[1][1];
        minus_RT_actor[1][2] = -RT_actor[2][1];
        minus_RT_actor[2][0] = -RT_actor[0][2];
        minus_RT_actor[2][1] = -RT_actor[1][2];
        minus_RT_actor[2][2] = -RT_actor[2][2];

        //std::cout << "Rotation matrix is" << std::endl;
        //std::cout << R_actor[0][0] << "," << R_actor[0][1] << "," << R_actor[0][2] << std::endl;
        //std::cout << R_actor[1][0] << "," << R_actor[1][1] << "," << R_actor[1][2] << std::endl;
        //std::cout << R_actor[2][0] << "," << R_actor[2][1] << "," << R_actor[2][2] << std::endl;
    }

    void evalCON() {

        //p_cmd[0] = 0;
        //p_cmd[1] = 0;
        //p_cmd[2] = 1;

        api.getContactPoints(myC, &contactInfo);
        N_con = contactInfo.m_numContactPoints;
        std::cout << "number of contacts are : " << N_con << std::endl;

        force[0] = 0;
        force[1] = 0;
        force[2] = 0;


        torque[0] = 0;
        torque[1] = 0;
        torque[2] = 0;

        f_prime[0] = 0;
        f_prime[1] = 0;
        f_prime[2] = 0;


        t_prime[0] = 0;
        t_prime[1] = 0;
        t_prime[2] = 0;

        if (N_con > 0)
        {
            //std::cout << contactInfo.m_contactPointData[j].m_bodyUniqueIdA << std::endl;
            //std::cout << contactInfo.m_contactPointData[j].m_contactDistance << std::endl;
          

            for (int k = 0; k < N_con; k++)
            {

                float curr_v1[3], curr_f1;
                float curr_v2[3], curr_f2;
                float curr_v3[3], curr_f3;
                float f_i[3];
                f_i[0] = 0; f_i[1] = 0; f_i[2] = 0;
                for (int dim = 0; dim < 3; dim++) {
                    curr_f1 = contactInfo.m_contactPointData[k].m_linearFrictionForce1;
                    curr_f2 = contactInfo.m_contactPointData[k].m_linearFrictionForce2;
                    curr_f3 = contactInfo.m_contactPointData[k].m_normalForce;
                    curr_v1[dim] = contactInfo.m_contactPointData[k].m_linearFrictionDirection1[dim];
                    curr_v2[dim] = contactInfo.m_contactPointData[k].m_linearFrictionDirection2[dim];
                    curr_v3[dim] = contactInfo.m_contactPointData[k].m_contactNormalOnBInWS[dim];

                    if (dim == 0) {
                        f_i[dim] = curr_f1 * curr_v1[dim];
                        force[0] = force[0] + f_i[dim];
                    }
                    if (dim == 1) {
                        f_i[dim] = curr_f2 * curr_v2[dim];
                        force[dim] = force[dim] + f_i[dim];
                    }
                    if (dim == 2) {
                        f_i[dim] = curr_f3 * curr_v3[dim];
                        force[dim] = force[dim] + f_i[dim];
                    }

                }

                positiononB[0] = contactInfo.m_contactPointData[k].m_positionOnBInWS[0];
                positiononB[1] = contactInfo.m_contactPointData[k].m_positionOnBInWS[1];
                positiononB[2] = contactInfo.m_contactPointData[k].m_positionOnBInWS[2];

                //std::cout << "contact location is: x = " << positiononB[0] << ", y = " << positiononB[1] << ", z = " << positiononB[2] << std::endl;
                //cross(positiononB,f_i)
                myCROSS(positiononB, f_i);

                torque[0] = cross_v1v2[0];
                torque[1] = cross_v1v2[1];
                torque[2] = cross_v1v2[2];

                //positiononB[0] - f_i[0]

                //std::cout << curr_f3 << std::endl;


                    // for all contacts
                    //torque_in_WS = ++cross(contactInfo.m_contactPointData[k].m_positionOnBInWS, force)

                //// position \in R3
                //contactInfo.m_contactPointData[k].m_positionOnBInWS;

                //// v1 \in R3
                //contactInfo.m_contactPointData[k].m_linearFrictionDirection1;
                //// v2 \in R3
                //contactInfo.m_contactPointData[k].m_linearFrictionDirection2;
                //// v3 \in R3
                //contactInfo.m_contactPointData[k].m_contactNormalOnBInWS;

                //// f1 \in R1
                //contactInfo.m_contactPointData[k].m_linearFrictionForce1;
                //// f2 \in R1
                //contactInfo.m_contactPointData[k].m_linearFrictionForce2;
                //// f3 \in R1
                //contactInfo.m_contactPointData[k].m_normalForce;


                /*f = contactInfo.m_contactPointData[k].m_normalForce;
                for (int l = 0; l < 3; l++) {
                    positiononA[l] = contactInfo.m_contactPointData[k].m_positionOnAInWS[l];
                    positiononB[l] = contactInfo.m_contactPointData[k].m_positionOnBInWS[l];
                }
                std::cout << "contact location on A in WS is : x =" << positiononA[0] << ": y = " << positiononA[1] << ": z = " << positiononA[2] << std::endl;
                std::cout << "contact location on B in WS is : x =" << positiononB[0] << ": y = " << positiononB[1] << ": z = " << positiononB[2] << std::endl;
                std::cout << "contact number " << k << " is : " << f << std::endl;*/
            }
            
        }

        getSE3();

        p_cross[0][0] = 0;
        p_cross[0][1] = -pos_actor[2];
        p_cross[0][2] = pos_actor[1];
        p_cross[1][0] = pos_actor[2];
        p_cross[1][1] = 0;
        p_cross[1][2] = -pos_actor[0];
        p_cross[2][0] = -pos_actor[1];
        p_cross[2][1] = pos_actor[0];
        p_cross[2][2] = 0;

        multiplyAx(RT_actor,force);
        f_prime[0] = Ax[0];
        f_prime[1] = Ax[1];
        f_prime[2] = Ax[2];

        multiplyAx(RT_actor, torque);
        t_prime[0] = 0 + Ax[0];
        t_prime[1] = 0 + Ax[1];
        t_prime[2] = 0 + Ax[2];

        multiplyAB(minus_RT_actor, p_cross);
        multiplyAx(AB, force);
        t_prime[0] = t_prime[0] + Ax[0];
        t_prime[1] = t_prime[1] + Ax[1];
        t_prime[2] = t_prime[2] + Ax[2];

        //std::cout << "Total force is : x = " << force[0] << ", y = " << force[1] << ", z = " << force[2] << std::endl;
        //std::cout << "Total torque is : x = " << torque[0] << ", y = " << torque[1] << ", z = " << torque[2] << std::endl;

        //std::cout << "Total f_prime is : x = " << f_prime[0] << ", y = " << f_prime[1] << ", z = " << f_prime[2] << std::endl;
        //std::cout << "Total t_prime is : x = " << t_prime[0] << ", y = " << t_prime[1] << ", z = " << t_prime[2] << std::endl;


    }

    myBULLET(float time_step_in) {
        time_step = time_step_in;
        init();
    }
};


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
    ARM RightARM("127.0.0.1#53210", myFORCEFACTOR, mySPEEDFACTOR, myDT);
    RightARM.name = "RightARM";
    RightARM.quick_start();
    RightARM.debug_getPOS();

    // Impedance control parameters
    float k = 0.1f, b = 0.1f;

    // Command structure object
    CMD cmd_R(k, b);

    for (int i = 0; i < 7; i++) {
        cmd_R.X_d[i] = RightARM.getPOS()[i];
    }

    int duration = 1000;
    myVIRTUOSE_LOG RightARM_LOG(duration); // duration input

    myWRITE_VIRT_LOG Right_LOG_writer(duration);

    int data_count = 0;



    myBULLET SIM(myDT);
    btVector3 pos;
    btQuaternion quat;

    float f_HAPTION_CMD[6];
    f_HAPTION_CMD[0] = 0;
        f_HAPTION_CMD[1] = 0;
        f_HAPTION_CMD[2] = 0;
        f_HAPTION_CMD[3] = 0;
        f_HAPTION_CMD[4] = 0;
        f_HAPTION_CMD[5] = 0;

    printf("Press Q to exit loop\n");
    while (!(GetKeyState('Q') & 0x8000)){

        // Haption stuff
        myUDP.UDP_send_recv_v3(RightARM.getPOS());

        //cmd_R.P_trn(cmd_R.X_d, RightARM.X); // needed if you wanted to do impedance control with Haption's initial state


        RightARM_LOG.write2LOG(data_count, cmd_R.X, cmd_R.f, myUDP.UDP_f);

        // RightARM.sendCMD_f(cmd_R.f); // issues the commanded force and resets the force variable also

        data_count = data_count + 1;



        // PyBullet stuff

        SIM.p_cmd[0] = RightARM.X[0];
        SIM.p_cmd[1] = RightARM.X[1];
        SIM.p_cmd[2] = RightARM.X[2];

        SIM.evalCON();

        printf("works\n");

        SIM.f_cmd[0] = SIM.k*(SIM.p_cmd[0] - SIM.pos_actor[0]) - SIM.b*(SIM.pdot_actor[0]);
        SIM.f_cmd[1] = SIM.k* (SIM.p_cmd[1] - SIM.pos_actor[1]) - SIM.b * (SIM.pdot_actor[1]);
        SIM.f_cmd[2] = SIM.k* (SIM.p_cmd[2] - SIM.pos_actor[2]) - SIM.b * (SIM.pdot_actor[2]);

        //SIM.getSE3();
        /*SIM.api.getBasePositionAndOrientation(SIM.actor, pos, quat);
        std::cout << "position - x = " << pos[0] << ", y = " << pos[1] << ", z = " << pos[2] << std::endl;
        std::cout << "quaternion - x = " << quat.getX() << ", y = " << quat.getY() << ", z = " << quat.getZ() << ", w = " << quat.getW() << std::endl;*/

        //SIM.quat_actor;
        //SIM.api.resetBasePositionAndOrientation(SIM.actor, btVector3(btScalar(0), btScalar(0), btScalar(0.1)), btQuaternion(btScalar(0), btScalar(0), btScalar(0), btScalar(1)));

        SIM.api.applyExternalForce(SIM.actor, -1, btVector3(btScalar(SIM.f_cmd[0]), btScalar(SIM.f_cmd[1]), btScalar(SIM.f_cmd[2])), btVector3(btScalar(0), btScalar(0), btScalar(0)), 0);
        
        f_HAPTION_CMD[0] = SIM.force[0];
        f_HAPTION_CMD[1] = SIM.force[1];
        f_HAPTION_CMD[2] = SIM.force[2];
        std::cout << "Haption command force is : x = " << f_HAPTION_CMD[0] << ", y = " << f_HAPTION_CMD[1] << ", z = " << f_HAPTION_CMD[2] << std::endl;
        RightARM.sendCMD_f(f_HAPTION_CMD); // issues the commanded force and resets the force variable also

        SIM.api.stepSimulation();
        Sleep(myDT);
    }

    // Haption clean up

    Right_LOG_writer.write2FILE(RightARM_LOG);

    myUDP.cleanup();
    RightARM.quick_stop();

    return 0;
}

