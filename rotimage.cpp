#include "rotimage.h"
#include <stdlib.h>
#include <math.h>
#include <QFileDialog>
#include <QMessageBox>
#include <QDateTime>
#include <QPen>
#include <QPainter>
#include <QMessageBox>
#include <QString>
#include <QPixmapCache>
#include <iostream>

#define MAX_WIDTH 640
#define MAX_HEIGHT 640
#define CENTROID_SAMPLE_COUNT 100000
#define OVERLAY_COLOR 0,0,200 // dead blue
#define GRABCUT_COLOR 200,0,0 // dead red


// tolerance, proportional to image hypotenuse, for grading

//#define TOLERANCE0 0.001
//#define TOLERANCE1 0.005
//#define TOLERANCE2 0.009
//#define TOLERANCE3 0.015
//#define TOLERANCE4 0.020
//#define TOLERANCE5 0.025
//#define TOLERANCE6 0.030
//#define TOLERANCE7 0.035
//#define TOLERANCE8 0.040
//#define TOLERANCE9 0.045
//#define TOLERANCE10 0.050
//#define TOLERANCE11 0.055
//#define TOLERANCE12 0.060
//#define TOLERANCE13 0.065
//#define TOLERANCE14 0.070
//#define TOLERANCE15 0.075
//#define TOLERANCE16 0.080
//#define TOLERANCE17 0.085
//#define TOLERANCE18 0.090
//#define TOLERANCE19 0.100
//#define TOLERANCE20 0.110

#define TOLERANCE0 0.001
#define TOLERANCE1 0.004
#define TOLERANCE2 0.008
#define TOLERANCE3 0.020
#define TOLERANCE4 0.040
#define TOLERANCE5 0.060
#define TOLERANCE6 0.080
#define TOLERANCE7 0.100

#define SCORE0 100
#define SCORE1 95
#define SCORE2 90
#define SCORE3 85
#define SCORE4 80
#define SCORE5 75
#define SCORE6 60
#define SCORE7 50
#define SCORE8 0

//#define SCORE0 100
//#define SCORE1 95
//#define SCORE2 90
//#define SCORE3 85
//#define SCORE4 80
//#define SCORE5 75
//#define SCORE6 70
//#define SCORE7 65
//#define SCORE8 60
//#define SCORE9 55
//#define SCORE10 50
//#define SCORE11 45
//#define SCORE12 40
//#define SCORE13 35
//#define SCORE14 30
//#define SCORE15 25
//#define SCORE16 20
//#define SCORE17 15
//#define SCORE18 10
//#define SCORE19 5
//#define SCORE20 0

using namespace cv;

ROTimage::ROTimage(QWidget *parent) : QLabel(parent)
{

}

int ROTimage::openFilename(){
   image.release();
    tmp.release();
   image = imread(QFileDialog::getOpenFileName(this,tr("Open Image"), "./images",
    tr("Image Files (*.jpeg; *.jpg; *.bmp; *.png)")).toStdString());
    if(!image.data || image.cols>MAX_WIDTH || image.rows>MAX_HEIGHT) {
        messagebox.setText(string.sprintf("Image not found or image resolution is greater than 640x480 or 480x640 pixels"));
        messagebox.exec();
        return(0);
    }
    else{
         // centroid variables
         intersect_x[4]=0, intersect_y[4]=0;
         centroid_x=0, centroid_y=0;
         x_accumulative=0, y_accumulative=0;
         x_temp=0, y_temp=0;
         count=0;
         image_centro.release();

        emit imageWidth(image.cols);
        emit imageHeight(image.rows);
        intersect_x[0] = image.cols/3;   intersect_y[0] = image.rows/3;
        intersect_x[1] = image.cols/3;   intersect_y[1] = 2*image.rows/3;
        intersect_x[2] = 2*image.cols/3; intersect_y[2] = image.rows/3;
        intersect_x[3] = 2*image.cols/3; intersect_y[3] = 2*image.rows/3;
        renderImage();
        return(0);
    }
}

void ROTimage::renderImage(){
    switch (image.type()) {
    case CV_8UC1:
        cvtColor(image, tmp, CV_GRAY2RGB);
        break;
    case CV_8UC3:
        cvtColor(image, tmp, CV_BGR2RGB);
        break;
    }
    assert(tmp.isContinuous());
    disp = QImage(tmp.data, tmp.cols, tmp.rows, tmp.cols*3, QImage::Format_RGB888);
    setPixmap(QPixmap::fromImage(disp));
    update();
}

void ROTimage::applyGrabcut(){

    try
    {
        // GrabCut segmentation
        grabCut(image,    // input image
                    result,   // segmentation result
                    grabcut_rect,// rectangle containing foreground
                    bgModel,fgModel, // models
                    1,        // number of iterations
                    GC_INIT_WITH_RECT); // use rectangle
        // Get the pixels marked as likely foreground
        compare(result,GC_PR_FGD,result,CMP_EQ);
        // Generate output image
        Mat foreground(image.size(),CV_8UC3,Scalar(0,0,0));
        image.copyTo(foreground,result); // bg pixels not copied
        image = foreground;
        renderImage();
    }

    catch(...)
    {
        messagebox.setText(string.sprintf("Open an image first before apply grabcut segmentation \
                                          or draw grabcut coordinates first"));
        messagebox.exec();
        return;
    }
}

void ROTimage::applyGrayOtsu(){
    try
    {
        Mat foreground(image.size(),CV_8UC3,Scalar(0,0,0));
        image.copyTo(foreground,result); // bg pixels not copied
        Mat img_gray;
        Mat img_bw;
        cvtColor(foreground,img_gray,CV_RGB2GRAY);
        threshold(img_gray,img_bw,0,255,CV_THRESH_BINARY|CV_THRESH_OTSU);
        image = img_bw;
        renderImage();
    }

    catch(...)
    {
        messagebox.setText(string.sprintf("Open an image first or apply the grabcut \
segmentation first"));
        messagebox.exec();
        return;
    }
}

void ROTimage::drawOverlay(){
    try
    {
        QPen pen;
        painter.begin(&disp);
        pen.setColor(qRgb(OVERLAY_COLOR));
        pen.setWidth(2);
        painter.setPen(pen);

        painter.begin(&disp);
        painter.setPen(qRgb(OVERLAY_COLOR));
        painter.drawLine(0, image.rows/3,   image.cols, image.rows/3);
        painter.drawLine(0, 2*image.rows/3, image.cols, 2*image.rows/3);
        painter.drawLine(image.cols/3,      0,  image.cols/3,   image.rows);
        painter.drawLine(2*image.cols/3,    0,  2*image.cols/3, image.rows);
        painter.end();
        setPixmap(QPixmap::fromImage(disp));
        update();
    }

    catch(...)
    {
        messagebox.setText(string.sprintf("Open an image first before use grabcut \
segmentation"));
        messagebox.exec();
        return;
    }
}

void ROTimage::drawGrabcut(){

    renderImage();
    QPen pen;
    painter.begin(&disp);
    pen.setColor(qRgb(GRABCUT_COLOR));
    pen.setWidth(2);
    painter.setPen(pen);

    painter.drawLine(grabcut_xbegin, grabcut_ybegin, grabcut_xbegin, grabcut_yend);
    painter.drawLine(grabcut_xend  , grabcut_ybegin, grabcut_xend  , grabcut_yend);
    painter.drawLine(grabcut_xbegin, grabcut_ybegin, grabcut_xend  , grabcut_ybegin);
    painter.drawLine(grabcut_xbegin, grabcut_yend  , grabcut_xend  , grabcut_yend);
    painter.end();
    setPixmap(QPixmap::fromImage(disp));
    update();
}

void ROTimage::setGrabcut_Xbegin(int pixel){
    grabcut_xbegin = pixel;
    grabcut_rect.x = grabcut_xbegin;
    drawGrabcut();
}

void ROTimage::setGrabcut_Ybegin(int pixel){
    grabcut_ybegin = pixel;
    grabcut_rect.y = grabcut_ybegin;
    drawGrabcut();
}

void ROTimage::setGrabcut_Xend(int pixel){
    grabcut_xend = pixel;
    grabcut_rect.width = grabcut_xend-grabcut_xbegin;
    drawGrabcut();
}

void ROTimage::setGrabcut_Yend(int pixel){
    grabcut_yend = pixel;
    grabcut_rect.height = grabcut_yend-grabcut_ybegin;
    drawGrabcut();
}

int ROTimage::checkRuleofThird(){
    try
    {
        while(count<CENTROID_SAMPLE_COUNT){
        x_temp = rand()%image.cols;
        y_temp = rand()%image.rows;
        Scalar color = image.at<uchar>(y_temp, x_temp);
        if (color.val[0]==255){ // the pixel[temp] is white!!
        x_accumulative = x_accumulative + double(x_temp);
        y_accumulative = y_accumulative + double(y_temp);
        count++;
            }
        }
        centroid_x = x_accumulative/ (double) CENTROID_SAMPLE_COUNT;
        centroid_y = y_accumulative/ (double) CENTROID_SAMPLE_COUNT;

        painter.begin(&disp);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setBrush(QBrush(Qt::red, Qt::SolidPattern));
        painter.drawEllipse(centroid_x,centroid_y,6,6);

        painter.end();
        setPixmap(QPixmap::fromImage(disp));
        update();

        qDebug("Centroid (x,y): %f %f",centroid_x,centroid_y);
        double hypotenuse = sqrt(pow(image.cols,2) + pow(image.rows,2));
        double distance;
        for (int i=0; i<4; i++){
            distance = sqrt(pow(centroid_x-intersect_x[i],2) \
                            + sqrt(pow(centroid_y-intersect_y[i],2)));
            if (distance < TOLERANCE0*hypotenuse){
                messagebox.setText(string.sprintf("Rule of Thirds: Yes.\nCentroid is at coordinates (%.2f, %.2f). \
                                                  \nDistance from nearest intersection is %.2f Pixel(s).\nScore %d."\
                                                  ,centroid_x,centroid_y,distance,SCORE0));
                messagebox.exec();
                return(0);
            }
            if (distance < TOLERANCE1*hypotenuse){
                messagebox.setText(string.sprintf("Rule of Thirds: Yes.\nCentroid is at coordinates (%.2f, %.2f). \
                                                  \nDistance from nearest intersection is %.2f Pixel(s).\nScore is %d."\
                                                  ,centroid_x,centroid_y,distance,SCORE1));
                messagebox.exec();
                return(0);
            }
            if (distance < TOLERANCE2*hypotenuse){
                messagebox.setText(string.sprintf("Rule of Thirds: Yes.\nCentroid is at coordinates (%.2f, %.2f). \
                                                  \nDistance from nearest intersection is %.2f Pixle(s).\nScore is %d."\
                                                  ,centroid_x,centroid_y,distance,SCORE2));
                messagebox.exec();

                return(0);
            }
            if (distance < TOLERANCE3*hypotenuse){
                messagebox.setText(string.sprintf("Rule of Thirds: Yes.\nCentroid is at coordinates (%.2f, %.2f). \
                                                  \nDistance from nearest intersection is %.2f Pixel(s).\nScore is %d."\
                                                  ,centroid_x,centroid_y,distance,SCORE3));
                messagebox.exec();
                return(0);
            }
            if (distance < TOLERANCE4*hypotenuse){
                messagebox.setText(string.sprintf("Rule of Thirds: Yes.\nCentroid is at coordinates (%.2f, %.2f). \
                                                  \nDistance from nearest intersection is %.2f Pixel(s).\nScore is %d."\
                                                  ,centroid_x,centroid_y,distance,SCORE4));
                messagebox.exec();
                return(0);
            }
            if (distance < TOLERANCE5*hypotenuse){
                messagebox.setText(string.sprintf("Rule of Thirds: Yes.\nCentroid is at coordinates (%.2f, %.2f). \
                                                  \nDistance from nearest intersection is %.2f Pixel(s).\nScore is %d."\
                                                  ,centroid_x,centroid_y,distance,SCORE5));
                messagebox.exec();
                return(0);
            }
            if (distance < TOLERANCE6*hypotenuse){
                messagebox.setText(string.sprintf("Rule of Thirds: Yes.\nCentroid is at coordinates (%.2f, %.2f). \
                                                  \nDistance from nearest intersection is %.2f Pixel(s).\nScore is %d."\
                                                  ,centroid_x,centroid_y,distance,SCORE6));
                messagebox.exec();
                return(0);
            }
            if (distance < TOLERANCE7*hypotenuse){
                messagebox.setText(string.sprintf("Rule of Thirds: Yes.\nCentroid is at coordinates (%.2f, %.2f). \
                                                  \nDistance from nearest intersection is %.2f Pixel(s).\nScore is %d."\
                                                  ,centroid_x,centroid_y,distance,SCORE7));
                messagebox.exec();
                return(0);
            }
}
//    if (distance < TOLERANCE8*hypotenuse){
//        messagebox.setText(string.sprintf("Rule of Thirds: Yes.\nCentroid is at coordinates (%.2f, %.2f). \
//                                          \nDistance from nearest intersection is %.2f Pixel(s).\nScore is %d."\
//                                          ,centroid_x,centroid_y,distance,SCORE8));
//        messagebox.exec();
//        return(0);
//    }

//    if (distance < TOLERANCE9*hypotenuse){
//        messagebox.setText(string.sprintf("Rule of Thirds: Yes.\nCentroid is at coordinates (%.2f, %.2f). \
//                                          \nDistance from nearest intersection is %.2f Pixel(s).\nScore is %d."\
//                                          ,centroid_x,centroid_y,distance,SCORE9));
//        messagebox.exec();
//        return(0);
//    }

//    if (distance < TOLERANCE10*hypotenuse){
//        messagebox.setText(string.sprintf("Rule of Thirds: Yes.\nCentroid is at coordinates (%.2f, %.2f). \
//                                          \nDistance from nearest intersection is %.2f Pixel(s).\nScore is %d."\
//                                          ,centroid_x,centroid_y,distance,SCORE10));
//        messagebox.exec();
//        return(0);
//    }

//    if (distance < TOLERANCE11*hypotenuse){
//        messagebox.setText(string.sprintf("Rule of Thirds: Yes.\nCentroid is at coordinates (%.2f, %.2f). \
//                                          \nDistance from nearest intersection is %.2f Pixel(s).\nScore is %d."\
//                                          ,centroid_x,centroid_y,distance,SCORE11));
//        messagebox.exec();
//        return(0);
//    }

//    if (distance < TOLERANCE12*hypotenuse){
//        messagebox.setText(string.sprintf("Rule of Thirds: Yes.\nCentroid is at coordinates (%.2f, %.2f). \
//                                          \nDistance from nearest intersection is %.2f Pixel(s).\nScore is %d."\
//                                          ,centroid_x,centroid_y,distance,SCORE12));
//        messagebox.exec();
//        return(0);
//    }

//    if (distance < TOLERANCE13*hypotenuse){
//        messagebox.setText(string.sprintf("Rule of Thirds: Yes.\nCentroid is at coordinates (%.2f, %.2f). \
//                                          \nDistance from nearest intersection is %.2f Pixel(s).\nScore is %d."\
//                                          ,centroid_x,centroid_y,distance,SCORE13));
//        messagebox.exec();
//        return(0);
//    }

//    if (distance < TOLERANCE14*hypotenuse){
//        messagebox.setText(string.sprintf("Rule of Thirds: Yes.\nCentroid is at coordinates (%.2f, %.2f). \
//                                          \nDistance from nearest intersection is %.2f Pixel(s).\nScore is %d."\
//                                          ,centroid_x,centroid_y,distance,SCORE14));
//        messagebox.exec();
//        return(0);
//    }

//    if (distance < TOLERANCE15*hypotenuse){
//        messagebox.setText(string.sprintf("Rule of Thirds: Yes.\nCentroid is at coordinates (%.2f, %.2f). \
//                                          \nDistance from nearest intersection is %.2f Pixel(s).\nScore is %d."\
//                                          ,centroid_x,centroid_y,distance,SCORE15));
//        messagebox.exec();
//        return(0);
//    }

//    if (distance < TOLERANCE16*hypotenuse){
//        messagebox.setText(string.sprintf("Rule of Thirds: Yes.\nCentroid is at coordinates (%.2f, %.2f). \
//                                          \nDistance from nearest intersection is %.2f Pixel(s).\nScore is %d."\
//                                          ,centroid_x,centroid_y,distance,SCORE16));
//        messagebox.exec();
//        return(0);
//    }

//    if (distance < TOLERANCE17*hypotenuse){
//        messagebox.setText(string.sprintf("Rule of Thirds: Yes.\nCentroid is at coordinates (%.2f, %.2f). \
//                                          \nDistance from nearest intersection is %.2f Pixel(s).\nScore is %d."\
//                                          ,centroid_x,centroid_y,distance,SCORE17));
//        messagebox.exec();
//        return(0);
//    }

//    if (distance < TOLERANCE18*hypotenuse){
//        messagebox.setText(string.sprintf("Rule of Thirds: Yes.\nCentroid is at coordinates (%.2f, %.2f). \
//                                          \nDistance from nearest intersection is %.2f Pixel(s).\nScore is %d."\
//                                          ,centroid_x,centroid_y,distance,SCORE18));
//        messagebox.exec();
//        return(0);
//    }

//    if (distance < TOLERANCE19*hypotenuse){
//        messagebox.setText(string.sprintf("Rule of Thirds: Yes.\nCentroid is at coordinates (%.2f, %.2f). \
//                                          \nDistance from nearest intersection is %.2f Pixel(s).\nScore is %d."\
//                                          ,centroid_x,centroid_y,distance,SCORE19));
//        messagebox.exec();
//        return(0);
//    }

//}
        messagebox.setText(string.sprintf("Rule of Thirds: No.\nCentroid is at coordinates (%.2f, %.2f). \
                                          \nDistance from nearest intersection is %.2f Pixel(s).\nScore is %d."\
                                          ,centroid_x,centroid_y,distance,SCORE8));
                messagebox.exec();
                return(0);
}


    catch(...)
    {
        messagebox.setText(string.sprintf("Open an image first before use grabcut \
segmentation"));
        messagebox.exec();
        return(0);
    }
}

int ROTimage::getHeight(){
    return(image.rows);
}

int ROTimage::getWidth(){
    return(image.cols);
}

//void ROTimage::exit()
//{
//    //exit(EXIT_FAILURE);
//}