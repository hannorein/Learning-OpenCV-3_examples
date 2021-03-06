#include "opencv2/imgcodecs.hpp"
#include "opencv2/imgproc.hpp"
#include "opencv2/videoio.hpp"
#include <opencv2/highgui.hpp>
#include <opencv2/tracking.hpp>
#include <opencv2/video.hpp>

#include <stdio.h>
#include <iostream>
#include <sstream>
#include <sys/time.h>

long getMicrotime(){
	struct timeval currentTime;
	gettimeofday(&currentTime, NULL);
	return currentTime.tv_sec * (int)1e6 + currentTime.tv_usec;
}


using namespace cv;
using namespace std;
Mat frame; //current frame
Mat mask; //fg mask fg mask generated by MOG2 method
Mat success;

Ptr<BackgroundSubtractorMOG2> pMOG2; //MOG2 Background subtractor
Ptr<Tracker> tracker;

long start_time = 0;
const int Nlog = 6;
long last_time[Nlog];
long last_time2[Nlog];
long last_pos[Nlog];
double last_speed[Nlog];
int first = 1;
long last_success = 0;
int Nsuccess;

int main(int argc, char* argv[])
{
    start_time = getMicrotime();
    //create GUI windows
    namedWindow("Frame");
    moveWindow("Frame", 0,0);
    namedWindow("Mask");
    moveWindow("Mask", 650,0);
    namedWindow("Last");
    moveWindow("Last", 650,450);

    //create Background Subtractor objects
    pMOG2 = createBackgroundSubtractorMOG2(); //MOG2 approach
    pMOG2->setDetectShadows(0);
    VideoCapture capture;
    capture.open(0);  
    capture.set(CV_CAP_PROP_FRAME_WIDTH,640);
    capture.set(CV_CAP_PROP_FRAME_HEIGHT,480);
    if(!capture.isOpened()){
        cerr << "Unable to open video stream." << endl;
        exit(EXIT_FAILURE);
    }

    Rect2d bbox(287, 23, 86, 320);
    
    double learning_rate = 0.01;
    int keyboard = 0;
    int first=1;
    long frames = 0;
    while( (char)keyboard != 'q' && (char)keyboard != 27 ){
        for (int i=0;i<2;i++){
        capture.grab();
        frames++;
        }
        long cur_time = getMicrotime();
        long cur_time2 = 1e6*((double)frames)/capture.get(CV_CAP_PROP_FPS);
        capture.retrieve(frame);
        //if(!capture.read(frame)) {
        //    cerr << "Unable to read next frame." << endl;
        //    exit(EXIT_FAILURE);
        //}
        //blur( frame, frame, Size( 2, 2 ), Point(-1,-1) );
        


        pMOG2->apply(frame, mask, learning_rate);


        Mat Points;
        blur( mask, mask, Size( 16, 16 ), Point(-1,-1) );
        findNonZero(mask-200,Points);
        Rect min_rect = boundingRect(Points);
        rectangle(frame, min_rect, Scalar( 255, 0, 0 ), 2, 1 );

        int cur_pos = -1;
        if (min_rect.x>0){
            cur_pos = min_rect.x;
        }
        //Mat sum;
        //reduce(mask, sum, 0, CV_REDUCE_SUM, CV_32S);
       
        //int min_length = 5;
        //int cur_pos = -1;
        //uint32_t max = 0;
        //for (int i=0;i<sum.cols-min_length;i++){
        //    uint32_t c = sum.at<uint32_t>(0,i);
        //    max = max<c?c:max;
        //}
        //if (max>5000){
        //    for (int i=0;i<sum.cols-min_length;i++){
        //        int passed = 1;
        //        for (int j=i;j<i+min_length;j++){
        //            if (sum.at<uint32_t>(0,i)<max/4){
        //                passed = 0;
        //            }
        //        }
        //        if (passed){
        //            cur_pos = i;
        //            break;
        //        }
        //    }
        //}
        double speed = 0;
        double speed2 = 0;
        if (cur_pos>0 && last_pos[Nlog-1]> cur_pos){
            speed =  1e6*((double)(last_pos[Nlog-1] - cur_pos))/((double)(cur_time-last_time[Nlog-1])); 
            speed2 =  1e6*((double)(last_pos[Nlog-1] - cur_pos))/((double)(cur_time2-last_time2[Nlog-1])); 
        }
        for (int i=1;i<Nlog;i++){
            last_time[i-1] = last_time[i];
            last_time2[i-1] = last_time2[i];
            last_speed[i-1] = last_speed[i];
            last_pos[i-1] = last_pos[i];
        }
        last_time[Nlog-1] = cur_time;
        last_time2[Nlog-1] = cur_time2;
        last_pos[Nlog-1] = cur_pos;
        last_speed[Nlog-1] = speed;
        double avg = 0;
        for (int i=0;i<Nlog;i++){
            avg+= last_speed[i];
        }
        avg /= (double) Nlog;
        int consistent = 1;
        for (int i=0;i<Nlog;i++){
            double rel = fabs((avg- last_speed[i])/avg);
            if (rel>1.9 || rel < 0.25){
                consistent = 0;
            }
        }


        if (speed){
            cout << (cur_time-start_time)/1e6 << "\t" << cur_pos << "\t" << speed << "\t" << speed2 << "\t" << consistent <<endl;
        }



        imshow("Frame", frame);
        imshow("Mask", mask);
        if ( first || (consistent && speed)){
            first = 0;

            success = frame.clone();
            
            for (int i=0;i<Nlog;i++){
                line(success, cvPoint(last_pos[i],0), cvPoint(last_pos[i],480), cvScalar(0,255,255));
            }
            char os[1024];


            time_t now = time(NULL);
            struct tm *t = localtime(&now);

            strftime(os, sizeof(os)-1, "%Y-%m-%d %H:%M", t);
            putText(success, os, cvPoint(30,420), FONT_HERSHEY_PLAIN, 2., cvScalar(0,0,255), 2, CV_AA);

            sprintf(os, "Event id: %05d   Speed: %.1f", Nsuccess, avg);
            putText(success, os, cvPoint(30,445), FONT_HERSHEY_PLAIN, 2., cvScalar(0,0,255), 2, CV_AA);

            imshow("Last", success);
            last_success = cur_time;
        }
        if (last_success && cur_time-last_success>2e6){
            last_success = 0;
            char filename[1024];
            sprintf(filename,"out/speed_%05d.jpg",Nsuccess);
            imwrite(filename, success);
            Nsuccess++;
        }
		keyboard = waitKey(1); 
    }
    capture.release();
    //destroy GUI windows
    destroyAllWindows();
    return EXIT_SUCCESS;
}
