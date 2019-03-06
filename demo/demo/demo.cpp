#include <iostream>
#include <signal.h>
#include <stdlib.h>     /* srand, rand */
#include <unistd.h>
#include <mutex>
#include "utils.h"

#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>

#include "Yolo3Detection.h"
#include "send.h"

#define MAX_DETECT_SIZE 100



bool gRun;

void sig_handler(int signo) {
    std::cout<<"request gateway stop\n";
    gRun = false;
}

int main(int argc, char *argv[]) {

    std::cout<<"detection\n";
    signal(SIGINT, sig_handler);


    char *net = "yolo3_berkeley.rt";
    if(argc > 1)
        net = argv[1]; 
    char *input = "../demo/yolo_test.mp4";
    if(argc > 2)
        input = argv[2]; 
    char *pmatrix = "/home/classfog1/repos/MASA_server/pmatrix/proj_matrix_20937.txt";
    if(argc > 3)
        pmatrix = argv[3];
    /*CAMID*/
    int CAM_IDX = 0;
    if(argc > 4)
        CAM_IDX = atoi(argv[4]);
    bool to_show = true;
    if(argc > 5)
        to_show = atoi(argv[5]);

    tk::dnn::Yolo3Detection yolo;
    yolo.init(net);
    yolo.thresh = 0.25;

    gRun = true;

    cv::VideoCapture cap(input);
    if(!cap.isOpened())
        gRun = false; 
    else
        std::cout<<"camera started\n";

    cv::Mat frame;
    cv::Mat dnn_input;
    cv::namedWindow("detection", cv::WINDOW_NORMAL);    

    /*projection matrix*/
    float* proj_matrix = (float*) malloc(9*sizeof(float));
    int proj_matrix_read = 0;

    /*socket*/
    int sock;
    int socket_opened = 0;
    
    struct obj_coords *coords = (struct obj_coords*)malloc(MAX_DETECT_SIZE*sizeof(struct obj_coords));

    while(gRun) {


        cap >> frame; 
        if(!frame.data) {
            continue;
        }  
 
        // this will be resized to the net format
        dnn_input = frame.clone();
        // TODO: async infer
        yolo.update(dnn_input);

        int coord_i = 0;
        int num_detected = yolo.detected.size();
        if (num_detected > MAX_DETECT_SIZE)
            num_detected = MAX_DETECT_SIZE;

        if(proj_matrix_read == 0)
            read_projection_matrix(proj_matrix, proj_matrix_read, pmatrix);
        
        /*printf("%f %f %f \n%f %f %f\n %f %f %f\n\n", proj_matrix[0],proj_matrix[1],
            proj_matrix[2],proj_matrix[3],proj_matrix[4],proj_matrix[5],
            proj_matrix[6],proj_matrix[7],proj_matrix[8]);*/

        // draw dets
        for(int i=0; i<num_detected; i++) {
            tk::dnn::box b = yolo.detected[i];
            int x0   = b.x;
            int x1   = b.x + b.w;
            int y0   = b.y;
            int y1   = b.y + b.h;
            int obj_class = b.cl;


            if(obj_class == 0 /*person*/ || obj_class == 1/*bicycle*/ || obj_class == 2/*car*/ 
                || obj_class == 3/*motorbike*/ || obj_class == 5/*bus*/)
            {
                convert_coords(coords, coord_i,x0+b.w/2, y1,obj_class, proj_matrix);
                coord_i++;
            }

            float prob = b.prob;

            std::cout<<obj_class<<" ("<<prob<<"): "<<x0<<" "<<y0<<" "<<x1<<" "<<y1<<"\n";
            cv::rectangle(frame, cv::Point(x0, y0), cv::Point(x1, y1), yolo.colors[obj_class], 2);                
        }

        send_client_dummy(coords, coord_i, sock, socket_opened, CAM_IDX);
    
        if (to_show)
       {
        cv::imshow("detection", frame);
        cv::waitKey(1);
       } 
    }

    free(coords);
    free(proj_matrix);

    std::cout<<"detection end\n";   
    return 0;
}
