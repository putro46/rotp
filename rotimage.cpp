#include "rotimage.h"
#include <stdlib.h>
#include <math.h>
#include <QFileDialog>
#include <QMessageBox>
#include <QDateTime>
#include <QPen>
#include <QPainter>
#include <QString>

#define MAX_WIDTH 640
#define MAX_HEIGHT 640
#define OVERLAY_COLOR 0,0,200 // dead blue
#define GRABCUT_COLOR 200,0,0 // dead red
//#define LINE_SCORE_GRADE -10

using namespace cv;

ROTimage::ROTimage(QWidget *parent) : QLabel(parent)
{

}

int ROTimage::openFilename(){
    image.release();
    tmp.release();

	tolerance[0] = 0.001;
	tolerance[1] = 0.008;
	tolerance[2] = 0.020;
	tolerance[3] = 0.030;
	tolerance[4] = 0.045;
	tolerance[5] = 0.060;
	tolerance[6] = 0.090;
	tolerance[7] = 0.100;

	score[0] = 100;
	score[1] = 95;
	score[2] = 90;
	score[3] = 85;
	score[4] = 80;
	score[5] = 75;
	score[6] = 60;
	score[7] = 50;
	score[8] = 0;

    image = imread(QFileDialog::getOpenFileName(this,tr("Open Image"), "./images",
                                                tr("Image Files (*.jpeg; *.jpg; *.bmp; *.png)")).toStdString());
    if(!image.data || image.cols>MAX_WIDTH || image.rows>MAX_HEIGHT) {
        messagebox.setText("Image not found or image dimension is greater than 640x640 pixels");
        messagebox.exec();
        return(0);
    }
    else{
        // centroid variables
        intersect_x[4]={ }, intersect_y[4]={ };
        centroid_x=0, centroid_y=0;
        x_accumulative=0, y_accumulative=0;
        x_temp=0, y_temp=0;
        count=0;

        emit newlyOpen();
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
                1, // number of iterations
                GC_INIT_WITH_RECT); // use rectangle
        // Get the pixels marked as likely foreground
        compare(result, GC_PR_FGD,result,CMP_EQ);
        // Generate output image
        Mat foreground(image.size(),CV_8UC3,Scalar(0,0,0));
        image.copyTo(foreground,result); // bg pixels not copied
        image = foreground;
        renderImage();
    }

    catch(...)
    {
        messagebox.setText("Open an image first before apply grabcut segmentation \
                           or draw grabcut coordinates first");
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
        messagebox.setText("Open an image first or apply the grabcut \
                           segmentation first");
                           messagebox.exec();
                return;
    }
}

void ROTimage::drawOverlay(){
    try
    {
        QPen pen;
        painter.begin(&disp);
        pen.setWidth(2);
        pen.setColor(qRgb(OVERLAY_COLOR));
        painter.setPen(pen);
        painter.drawLine(0, image.rows/3,   image.cols, image.rows/3); //draw upper horizontal third line
        painter.drawLine(0, 2*image.rows/3, image.cols, 2*image.rows/3); //draw lower horizontal third line
        painter.drawLine(image.cols/3,      0,  image.cols/3,   image.rows); //draw left vertical third line
        painter.drawLine(2*image.cols/3,    0,  2*image.cols/3, image.rows); //draw right vertical third line
        painter.end();
        setPixmap(QPixmap::fromImage(disp));
        update();
    }

    catch(...)
    {
        messagebox.setText("Open an image first before use grabcut \
                           segmentation");
                           messagebox.exec();
                return;
    }
}

void ROTimage::drawGrabcutBoundary(){

    renderImage();
    QPen pen;
    painter.begin(&disp);
    pen.setColor(qRgb(GRABCUT_COLOR));
    pen.setWidth(4);
    painter.setPen(pen);

    painter.drawLine(grabcut_xbegin, grabcut_ybegin, grabcut_xbegin, grabcut_yend); //draw left vertical line
    painter.drawLine(grabcut_xend  , grabcut_ybegin, grabcut_xend  , grabcut_yend); //draw right vertical line
    painter.drawLine(grabcut_xbegin, grabcut_ybegin, grabcut_xend  , grabcut_ybegin); //draw upper horizontal line
    painter.drawLine(grabcut_xbegin, grabcut_yend  , grabcut_xend  , grabcut_yend); //drraw lower horizontal line
    painter.end();
    setPixmap(QPixmap::fromImage(disp));
    update();
}

void ROTimage::setGrabcut_Xbegin(int pixel){
    grabcut_xbegin = pixel;
    grabcut_rect.x = grabcut_xbegin;
    drawGrabcutBoundary();
}

void ROTimage::setGrabcut_Ybegin(int pixel){
    grabcut_ybegin = pixel;
    grabcut_rect.y = grabcut_ybegin;
    drawGrabcutBoundary();
}

void ROTimage::setGrabcut_Xend(int pixel){
    grabcut_xend = pixel;
    grabcut_rect.width = grabcut_xend-grabcut_xbegin;
    drawGrabcutBoundary();
}

void ROTimage::setGrabcut_Yend(int pixel){
    grabcut_yend = pixel;
    grabcut_rect.height = grabcut_yend-grabcut_ybegin;
    drawGrabcutBoundary();
}

int ROTimage::checkRuleofThirds(){

    //sampling size formula
    grab_populate = (grabcut_xend-grabcut_xbegin)*(grabcut_yend-grabcut_ybegin);
    whole_populate = image.cols*image.rows;
    proportion = grab_populate/whole_populate;
    q = 1 - proportion;
    trust = 1.96;
    tpq = pow(trust,2)*proportion*q;
    deviation = pow(0.05,2);
    min_sample = (tpq/deviation)/(1+(1/whole_populate)*((q/deviation)-1));
    qDebug("grabcut populate is %f and minimum sample is %f and tpq is %f",grab_populate,min_sample,tpq);

    try
    {
        //centroid determine formula
        srand(QDateTime::currentMSecsSinceEpoch());
        while(count <= min_sample){

            x_temp = grabcut_xbegin + rand()%(grabcut_xend-grabcut_xbegin);
            y_temp = grabcut_ybegin + rand()%(grabcut_yend-grabcut_ybegin);
            //qDebug("xtemp = %f and ytemp = %f",x_temp,y_temp);
            Scalar color = image.at<uchar>(y_temp, x_temp);
            if (color.val[0]==255){ // the pixel[temp] is white!!
                x_accumulative = x_accumulative + x_temp;
                y_accumulative = y_accumulative + y_temp;
                count++;
            }
        }
        //averaging the accumulative result with the number of sample
        centroid_x = x_accumulative/min_sample;
        centroid_y = y_accumulative/min_sample;

        //draw centroid mark
        painter.begin(&disp);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setBrush(QBrush(Qt::red, Qt::SolidPattern));
        painter.drawEllipse(centroid_x-5,centroid_y-5,10,10);
        painter.end();
        setPixmap(QPixmap::fromImage(disp));
        update();

        qDebug("Centroid (x,y): %f %f",centroid_x,centroid_y);

        image_hypotenuse = sqrt(pow(image.cols,2) + pow(image.rows,2));

        // rule of thirds check, intersection rule
        for (int i=0; i<4; i++){

            //check rule of thirds intersection pass thru
            Scalar color = image.at<uchar>(intersect_x[i], intersect_y[i]);
            if (color.val[0]==255){ // the pixel[temp] is white!!
                painter.begin(&disp);
                painter.setRenderHint(QPainter::Antialiasing, true);
                painter.setBrush(QBrush(Qt::yellow, Qt::SolidPattern));
                painter.drawEllipse(intersect_x[i]-5,intersect_y[i]-5,10,10);
                painter.end();
                setPixmap(QPixmap::fromImage(disp));
                update();

                qDebug("Rule Of Thirds: Yes. Pass thru at (%.2f, %.2f)",intersect_x[i],intersect_y[i]);
            }

            QPen pen;
            painter.begin(&disp);
            pen.setWidth(2);
            pen.setStyle(Qt::DashLine);
            pen.setColor(qRgb(0,255,0));
            painter.setPen(pen);
            painter.drawLine(centroid_x, centroid_y, intersect_x[i],intersect_y[i]); //draw dashed line from centroid to all intersection
            painter.end();
            setPixmap(QPixmap::fromImage(disp));
            update();

            for (int j=0; j<8; j++){
                distance_hypotenuse = sqrt(pow(centroid_x-intersect_x[i],2)+pow(centroid_y-intersect_y[i],2));

                if (distance_hypotenuse < tolerance[j]*image_hypotenuse){
                    messagebox.setText(string.sprintf("Rule of Thirds: Yes. \
                                                      \nCentroid is at coordinates (%.2f, %.2f). \
                                                      \nPass thru rule of thirds point at (%.2f, %.2f). \
                                                      \nPopulation in grabcut area is %.0f. \
                                                      \nNumber of sample is %.0f. \
                                                      \nDistance from nearest intersection is %.2f Pixel(s). \
                                                      \nScore %d.",
                                                      centroid_x, centroid_y,intersect_x[i],intersect_y[i],grab_populate, \
                                                      min_sample, distance_hypotenuse, score[j]));
                    //                                                      centroid_x, centroid_y, count,population, distance, score[j]));
                    messagebox.exec();
                    return(0);
                }
            }
        }

        // rule of thirds check, line rule
        //        for (int i=0; i<4; i++){
        //            switch (i) {
        //            case 1:
        //                distance = abs(centroid_x-image.cols/3);
        //                break;
        //            case 2:
        //                distance = abs(centroid_x-2*image.cols/3);
        //                break;
        //            case 3:
        //                distance = abs(centroid_y-image.rows/3);
        //                break;
        //            case 4:
        //                distance = abs(centroid_y-2*image.rows/3);
        //                break;
        //            }
        //            for (int j=0; j<8; j++){
        //                if (distance < tolerance[j]*hypotenuse){
        //                    messagebox.setText(string.sprintf("Rule of Thirds: Yes (Line Rule). \
        //                                                      \nCentroid is at coordinates (%.2f, %.2f). Sample count: %d/%d\
        //                                                      \nDistance from nearest line is %.2f Pixel(s). Score %d.",
        //                                                      centroid_x, centroid_y, count, population, distance, score[j]+LINE_SCORE_GRADE));
        //                    messagebox.exec();
        //                    return(0);
        //                }
        //            }
        //        }

        for (int i=0; i<4; i++){

            Scalar color = image.at<uchar>(intersect_x[i], intersect_y[i]);
            if (color.val[0]==255){ // the pixel[temp] is white!!
                painter.begin(&disp);
                painter.setRenderHint(QPainter::Antialiasing, true);
                painter.setBrush(QBrush(Qt::yellow, Qt::SolidPattern));
                painter.drawEllipse(intersect_x[i],intersect_y[i],10,10);
                painter.end();
                setPixmap(QPixmap::fromImage(disp));
                update();

                messagebox.setText(string.sprintf("Rule of Thirds: No. \
                                                  \nCentroid is at coordinates (%.2f, %.2f). \
                                                  \nPass thru rule of thirds point at (%.2f, %.2f). \
                                                  \nPopulation in grabcut area is %.0f. \
                                                  \nNumber of sample is %.0f. \
                                                  \nDistance from nearest intersection is %.2f Pixel(s). \
                                                  \nScore %d.",
                                                  centroid_x, centroid_y,intersect_x[i],intersect_y[i],grab_populate, \
                                                  min_sample, distance_hypotenuse, score[8]));
                messagebox.exec();
                return(0);
            }
        }
    }

    catch(...){
        messagebox.setText("Open an image first before use grabcut segmentation");
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
