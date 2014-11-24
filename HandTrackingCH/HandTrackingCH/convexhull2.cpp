
// Background average sample code done with averages and done with codebooks
/* 
************************************************** */
#include <cv.h>
#include "cvaux.h"
#include "cxmisc.h"
#include "highgui.h"
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <algorithm>
#include <fstream>

//VARIABLES for CODEBOOK METHOD:
CvBGCodeBookModel* model = 0;
const int NCHANNELS = 3;
bool ch[NCHANNELS]={true,true,true}; // This sets what channels should be adjusted for background bounds
bool saveLength = false;

void  detect(IplImage* img_8uc1,IplImage* img_8uc3);

double getMin(double a, double b, double c, double d);
double getMax(double a, double b, double c, double d);
double longBtwnPoints (CvPoint a, CvPoint b);
char identificaGesto (double longitud, int numDef, double radio);


void help(void)
{
    printf("\nLearn background and find foreground using simple average and average difference learning method:\n"
        "\nUSAGE:\nbgfg_codebook [--nframes=300] [movie filename, else from camera]\n"
        "***Keep the focus on the video windows, NOT the consol***\n\n"
        "INTERACTIVE PARAMETERS:\n"
        "\tESC,q,Q  - quit the program\n"
        "\th	- print this help\n"
        "\tp	- pause toggle\n"
        "\ts	- single step\n"
        "\tr	- run mode (single step off)\n"
        "=== AVG PARAMS ===\n"
        "\t-    - bump high threshold UP by 0.25\n"
        "\t=    - bump high threshold DOWN by 0.25\n"
        "\t[    - bump low threshold UP by 0.25\n"
        "\t]    - bump low threshold DOWN by 0.25\n"
        "=== CODEBOOK PARAMS ===\n"
        "\ty,u,v- only adjust channel 0(y) or 1(u) or 2(v) respectively\n"
        "\ta	- adjust all 3 channels at once\n"
        "\tb	- adjust both 2 and 3 at once\n"
        "\ti,o	- bump upper threshold up,down by 1\n"
        "\tk,l	- bump lower threshold up,down by 1\n"
        "\tSPACE - reset the model\n"
        );
}

//
//USAGE:  ch9_background startFrameCollection# endFrameCollection# [movie filename, else from camera]
//If from AVI, then optionally add HighAvg, LowAvg, HighCB_Y LowCB_Y HighCB_U LowCB_U HighCB_V LowCB_V
//
int main(int argc, char** argv)
{
    const char* filename = 0;
    IplImage* rawImage = 0, *yuvImage = 0; //yuvImage is for codebook method
    IplImage *ImaskCodeBook = 0,*ImaskCodeBookCC = 0;
    CvCapture* capture = 0;
	

	//cvRect ROIArea = cvRect(0,0,200,200);

    int c, n, nframes = 0;
    int nframesToLearnBG = 300;

    model = cvCreateBGCodeBookModel();
    
    //Set color thresholds to default values
    model->modMin[0] = 3;
    model->modMin[1] = model->modMin[2] = 3;
    model->modMax[0] = 10;
    model->modMax[1] = model->modMax[2] = 10;
    model->cbBounds[0] = model->cbBounds[1] = model->cbBounds[2] = 10;

    bool pause = false;
    bool singlestep = false;

    for( n = 1; n < argc; n++ )
    {
        static const char* nframesOpt = "--nframes=";
        if( strncmp(argv[n], nframesOpt, strlen(nframesOpt))==0 )
        {
            if( sscanf(argv[n] + strlen(nframesOpt), "%d", &nframesToLearnBG) == 0 )
            {
                help();
                return -1;
            }
        }
        else
            filename = argv[n];
    }

    if( !filename )
    {
        printf("Capture from camera\n");
        capture = cvCaptureFromCAM( 0 );
    }
    else
    {
        printf("Capture from file %s\n",filename);
        capture = cvCreateFileCapture( filename );
    }

    if( !capture )
    {
        printf( "Can not initialize video capturing\n\n" );
        help();
        return -1;
    }

    //MAIN PROCESSING LOOP:
    for(;;)
    {
        if( !pause )
        {
            rawImage = cvQueryFrame( capture );
            ++nframes;
            if(!rawImage) 
                break;
        }
        if( singlestep )
            pause = true;
        
        //First time:
        if( nframes == 1 && rawImage )
        {
            // CODEBOOK METHOD ALLOCATION
            yuvImage = cvCloneImage(rawImage);

			float w = yuvImage->width;
			cvSetImageROI(yuvImage, cvRect(w-250,0,250,250));
			IplImage *tmp = cvCreateImage(cvGetSize(yuvImage),yuvImage->depth,yuvImage->nChannels);
			cvCopy(yuvImage, tmp, NULL);
			cvResetImageROI(yuvImage);
			yuvImage = cvCloneImage(tmp);

			ImaskCodeBook = cvCreateImage( cvGetSize(yuvImage), IPL_DEPTH_8U, 1 );
            ImaskCodeBookCC = cvCreateImage( cvGetSize(yuvImage), IPL_DEPTH_8U, 1 );

            //ImaskCodeBook = cvCreateImage( cvGetSize(rawImage), IPL_DEPTH_8U, 1 );
            //ImaskCodeBookCC = cvCreateImage( cvGetSize(rawImage), IPL_DEPTH_8U, 1 );
            cvSet(ImaskCodeBook,cvScalar(255));
            
			cvNamedWindow("Raw",CV_WINDOW_AUTOSIZE);
            cvNamedWindow( "ForegroundCodeBook",CV_WINDOW_AUTOSIZE);
            cvNamedWindow( "CodeBook_ConnectComp",CV_WINDOW_AUTOSIZE);
        }

        // If we've got an rawImage and are good to go:                
        if( rawImage )
        {
			cvFlip(rawImage, NULL, 1);
            float w = rawImage->width;

			//Dibujar contorno
			 cvLine(rawImage, cv::Point (w-250,0), cv::Point (w-250,250), CV_RGB(255,0,0),1, CV_AA, 0) ;
			 cvLine(rawImage, cv::Point (w-250,250), cv::Point (w,250), CV_RGB(255,0,0),1, CV_AA, 0) ;
			//
			if(nframes - 1 < nframesToLearnBG){
				char buffer [33];
				itoa (nframesToLearnBG - nframes - 1,buffer,10);
				CvFont font2;
				cvInitFont(&font2, CV_FONT_HERSHEY_SIMPLEX, 1.0, 1.0, 0, 3, CV_AA);
				cvPutText(rawImage, buffer, cvPoint(50, 50), &font2, cvScalar(0, 0, 255, 0));
			}

			cvSetImageROI(rawImage, cvRect(w-250,0,250,250));
			IplImage *temp = cvCreateImage(cvGetSize(rawImage),rawImage->depth,rawImage->nChannels);
			
			cvCvtColor( rawImage, yuvImage, CV_BGR2YCrCb );
			//YUV For codebook method
			
			//This is where we build our background model
            if( !pause && nframes-1 < nframesToLearnBG  )
			{
				

                cvBGCodeBookUpdate( model, yuvImage );
				 
			}

            if( nframes-1 == nframesToLearnBG  )
                cvBGCodeBookClearStale( model, model->t/2 );
            
            //Find the foreground if any
            if( nframes-1 >= nframesToLearnBG  )
            {
                // Find foreground by codebook method
                cvBGCodeBookDiff( model, yuvImage, ImaskCodeBook );
                // This part just to visualize bounding boxes and centers if desired
                cvCopy(ImaskCodeBook,ImaskCodeBookCC);	
                cvSegmentFGMask( ImaskCodeBookCC );
                //bwareaopen_(ImaskCodeBookCC,100);
                cvShowImage( "CodeBook_ConnectComp",ImaskCodeBookCC);
                detect(ImaskCodeBookCC,rawImage);
                
            }
            //Display
			cvResetImageROI(rawImage);
            cvShowImage( "Raw", rawImage );
            cvShowImage( "ForegroundCodeBook",ImaskCodeBook);
            
        }

        // User input:
        c = cvWaitKey(10)&0xFF;
        c = tolower(c);
        // End processing on ESC, q or Q
        if(c == 27 || c == 'q')
            break;
        //Else check for user input
        switch( c )
        {
		case 'c':
			saveLength = true;
			break;
        case 'h':
            help();
            break;
        case 'p':
            pause = !pause;
            break;
        case 's':
            singlestep = !singlestep;
            pause = false;
            break;
        case 'r':
            pause = false;
            singlestep = false;
            break;
        case ' ':
            cvBGCodeBookClearStale( model, 0 );
            nframes = 0;
            break;
            //CODEBOOK PARAMS
        case 'y': case '0':
        case 'u': case '1':
        case 'v': case '2':
        case 'a': case '3':
        case 'b': 
            ch[0] = c == 'y' || c == '0' || c == 'a' || c == '3';
            ch[1] = c == 'u' || c == '1' || c == 'a' || c == '3' || c == 'b';
            ch[2] = c == 'v' || c == '2' || c == 'a' || c == '3' || c == 'b';
            printf("CodeBook YUV Channels active: %d, %d, %d\n", ch[0], ch[1], ch[2] );
            break;
        case 'i': //modify max classification bounds (max bound goes higher)
        case 'o': //modify max classification bounds (max bound goes lower)
        case 'k': //modify min classification bounds (min bound goes lower)
        case 'l': //modify min classification bounds (min bound goes higher)
            {
            uchar* ptr = c == 'i' || c == 'o' ? model->modMax : model->modMin;
            for(n=0; n<NCHANNELS; n++)
            {
                if( ch[n] )
                {
                    int v = ptr[n] + (c == 'i' || c == 'l' ? 1 : -1);
                    ptr[n] = CV_CAST_8U(v);
                }
                printf("%d,", ptr[n]);
            }
            printf(" CodeBook %s Side\n", c == 'i' || c == 'o' ? "High" : "Low" );
            }
            break;
        }

		if (c != 'c')
			saveLength=false;
    }		
    
    cvReleaseCapture( &capture );
    cvDestroyWindow( "Raw" );
    cvDestroyWindow( "ForegroundCodeBook");
    cvDestroyWindow( "CodeBook_ConnectComp");
    return 0;
}
/*
void  detect(IplImage* img_8uc1,IplImage* img_8uc3) 
{
	//cvNamedWindow( "aug", 1 );

	//cvThreshold( img_8uc1, img_edge, 128, 255, CV_THRESH_BINARY );
	CvMemStorage* storage = cvCreateMemStorage();
	CvSeq* first_contour = NULL;
	CvSeq* maxitem=NULL;
	double area=0,areamax=0;
	int maxn=0;
	int Nc = cvFindContours(
							img_8uc1,
							storage,
							&first_contour,
							sizeof(CvContour),
							CV_RETR_LIST // Try all four values and see what happens
							);
	int n=0;
	//printf( "Total Contours Detected: %d\n", Nc );

	if(Nc>0)
	{
		for( CvSeq* c=first_contour; c!=NULL; c=c->h_next ) 
		{     
			//cvCvtColor( img_8uc1, img_8uc3, CV_GRAY2BGR );

			area=cvContourArea(c,CV_WHOLE_SEQ );

			if(area>areamax)
			{
				areamax=area;
				maxitem=c;
				maxn=n;
			}
			n++;
		}
		
		if(areamax>5000)
		{
			CvPoint pt0;

			CvMemStorage* storage1 = cvCreateMemStorage();
			CvSeq* ptseq = cvCreateSeq( CV_SEQ_KIND_GENERIC|CV_32SC2, sizeof(CvContour), sizeof(CvPoint), storage1 );
			CvSeq* hull;

			for(int i = 0; i < maxitem->total; i++ )
			{   
				CvPoint* p = CV_GET_SEQ_ELEM( CvPoint, maxitem, i );
				pt0.x = p->x;
				pt0.y = p->y;
				cvSeqPush( ptseq, &pt0 );
			}
			hull = cvConvexHull2( ptseq, 0, CV_CLOCKWISE, 0 );
			int hullcount = hull->total;



			pt0 = **CV_GET_SEQ_ELEM( CvPoint*, hull, hullcount - 1 );

			for(int i = 0; i < hullcount; i++ )
			{

				CvPoint pt = **CV_GET_SEQ_ELEM( CvPoint*, hull, i );
				cvLine( img_8uc3, pt0, pt, CV_RGB( 0, 255, 0 ), 1, CV_AA, 0 );
				pt0 = pt;
			}
			
			cvReleaseMemStorage( &storage );
			cvReleaseMemStorage( &storage1 );
			//return 0;
		}
	}
}*/


void  detect(IplImage* img_8uc1,IplImage* img_8uc3) 
{  
    
//cvNamedWindow( "aug", 1 );


//cvThreshold( img_8uc1, img_edge, 128, 255, CV_THRESH_BINARY );
	CvMemStorage* storage = cvCreateMemStorage();
	CvSeq* first_contour = NULL;
	CvSeq* maxitem=NULL;
	char resultado [] = " ";
	double area=0,areamax=0;
	double longitud = 0;
	double radio = 0;
	int maxn=0;
	int Nc = cvFindContours(
	img_8uc1,
	storage,
	&first_contour,
	sizeof(CvContour),
	CV_RETR_LIST // Try all four values and see what happens
	);
	int n=0;
	//printf( "Total Contours Detected: %d\n", Nc );

	if(Nc>0)
	{
		for( CvSeq* c=first_contour; c!=NULL; c=c->h_next ) 
		{     
			//cvCvtColor( img_8uc1, img_8uc3, CV_GRAY2BGR );
			area=cvContourArea(c,CV_WHOLE_SEQ );

			if(area>areamax)
			{
				areamax=area;
				maxitem=c;
				maxn=n;
			}
			n++;
		}
		
		CvMemStorage* storage3 = cvCreateMemStorage(0);
		//if (maxitem) maxitem = cvApproxPoly( maxitem, sizeof(maxitem), storage3, CV_POLY_APPROX_DP, 3, 1 );  
		
		if(areamax>5000)
		{
			maxitem = cvApproxPoly( maxitem, sizeof(CvContour), storage3, CV_POLY_APPROX_DP, 10, 1 );
			CvPoint pt0;
			
			CvMemStorage* storage1 = cvCreateMemStorage(0);
			CvMemStorage* storage2 = cvCreateMemStorage(0);
			CvSeq* ptseq = cvCreateSeq( CV_SEQ_KIND_GENERIC|CV_32SC2, sizeof(CvContour), sizeof(CvPoint), storage1 );
			CvSeq* hull;
			CvSeq* defects;

			CvPoint minDefectPos;;
			minDefectPos.x = 1000000;
			minDefectPos.y = 1000000;

			CvPoint maxDefectPos;
			maxDefectPos.x = 0;
			maxDefectPos.y = 0;			
			

			for(int i = 0; i < maxitem->total; i++ )
			{   
				CvPoint* p = CV_GET_SEQ_ELEM( CvPoint, maxitem, i );
				pt0.x = p->x;
				pt0.y = p->y;
				cvSeqPush( ptseq, &pt0 );
			}
			hull = cvConvexHull2( ptseq, 0, CV_CLOCKWISE, 0 );
			int hullcount = hull->total;
        
			defects= cvConvexityDefects(ptseq,hull,storage2  );

			//printf(" defect no %d \n",defects->total);
			CvConvexityDefect* defectArray;  
			
			int j=0;  
			//int m_nomdef=0;
			// This cycle marks all defects of convexity of current contours.  

			longitud = 0;

			for(;defects;defects = defects->h_next)  
			{  
				int nomdef = defects->total; // defect amount  
				//outlet_float( m_nomdef, nomdef );  
				//printf(" defect no %d \n",nomdef);
				
				if(nomdef == 0)  
                continue;  
				
				// Alloc memory for defect set.     
				//fprintf(stderr,"malloc\n");  
				defectArray = (CvConvexityDefect*)malloc(sizeof(CvConvexityDefect)*nomdef);  
              
				// Get defect set.  
				//fprintf(stderr,"cvCvtSeqToArray\n");  
				cvCvtSeqToArray(defects,defectArray, CV_WHOLE_SEQ); 
				
				

				

				

				// Draw marks for all defects.  
				for(int i=0; i<nomdef; i++)  
				{  
					CvPoint startP;
					startP.x = defectArray[i].start->x;
					startP.y = defectArray[i].start->y;

					CvPoint depthP;
					depthP.x = defectArray[i].depth_point->x;
					depthP.y = defectArray[i].depth_point->y;

					CvPoint endP;
					endP.x = defectArray[i].end->x;
					endP.y = defectArray[i].end->y;

					

					//obtener minimo y maximo

					minDefectPos.x = getMin (startP.x, depthP.x, endP.x, minDefectPos.x);
					minDefectPos.y = getMin (startP.y, depthP.y, endP.y, minDefectPos.y);

					maxDefectPos.x = getMax (startP.x, depthP.x, endP.x, maxDefectPos.x);
					maxDefectPos.y = getMax (startP.y, depthP.y, endP.y, maxDefectPos.y);					
					
					//fin obtener minimo y maximo
					if (saveLength)
					{
						longitud += longBtwnPoints(startP, depthP);
						longitud += longBtwnPoints(depthP, endP);
						
					}
					//printf(" defect depth for defect %d %f \n",i,defectArray[i].depth);
					cvLine(img_8uc3, startP, depthP, CV_RGB(255,255,0),1, CV_AA, 0 ); 

					
					cvCircle( img_8uc3, depthP, 5, CV_RGB(0,0,164), 2, 8,0);  
					cvCircle( img_8uc3, startP, 5, CV_RGB(255,0,0), 2, 8,0);  
					cvCircle( img_8uc3, endP, 5, CV_RGB(0,255,0), 2, 8,0);  

					cvLine(img_8uc3, depthP, endP,CV_RGB(0,0,0),1, CV_AA, 0 );   
				} 

				if (nomdef>0)
				{
						resultado [0] = identificaGesto (longitud, nomdef, radio);
						if (resultado[0] !=' ')
							printf ("Gesto identificado (%c) \n", resultado[0]);
				}

				if (saveLength)
				{
					radio = (double)maxDefectPos.x / (double)maxDefectPos.y;
					if (nomdef>0)
					{
						printf ("_______________________\n");
						resultado [0] = identificaGesto (longitud, nomdef, radio);
						if (resultado[0] !=' ')
							printf ("Gesto identificado (%c) \n", resultado[0]);
						else
							printf ("No se identifico ningun gesto\n");
						printf(" Longitud %g \n NomDef %i \n radio %g \n",longitud, nomdef, radio);
					FILE *fp;
					fp=fopen("archivo.txt", "a");
					fprintf(fp, "\n%g\n%i\n%g\n",longitud, nomdef, radio);
					fclose (fp);
					}
					else
						printf("No hay defectos");
					printf ("_______________________\n");
				}
				/*
				char txt[]="0";
				txt[0]='0'+nomdef-1;
				CvFont font;
				cvInitFont(&font, CV_FONT_HERSHEY_SIMPLEX, 1.0, 1.0, 0, 5, CV_AA);
				cvPutText(img_8uc3, txt, cvPoint(50, 50), &font, cvScalar(0, 0, 255, 0)); */

				CvFont font;
				cvInitFont(&font, CV_FONT_HERSHEY_SIMPLEX, 1.0, 1.0, 0, 5, CV_AA);
				if (resultado!= NULL)
					cvPutText(img_8uc3, resultado, cvPoint(50, 50), &font, cvScalar(0, 0, 255, 0));
				
				j++;  
				
				// Free memory.   
				free(defectArray);  
			} 

			pt0 = **CV_GET_SEQ_ELEM( CvPoint*, hull, hullcount - 1 );

			for(int i = 0; i < hullcount; i++ )
			{

				CvPoint pt = **CV_GET_SEQ_ELEM( CvPoint*, hull, i );
				cvLine( img_8uc3, pt0, pt, CV_RGB( 0, 255, 0 ), 1, CV_AA, 0 );
				pt0 = pt;
			}

			

			cvLine( img_8uc3, minDefectPos, cvPoint( (maxDefectPos.x), (minDefectPos.y)), CV_RGB( 2500, 0, 0 ), 1, CV_AA, 0 );
			cvLine( img_8uc3,  cvPoint( (maxDefectPos.x), (minDefectPos.y)), maxDefectPos, CV_RGB( 2500, 0, 0 ), 1, CV_AA, 0 );
			cvLine( img_8uc3, maxDefectPos, cvPoint( (minDefectPos.x), (maxDefectPos.y)), CV_RGB( 2500, 0, 0 ), 1, CV_AA, 0 );
			cvLine( img_8uc3, cvPoint( (minDefectPos.x), (maxDefectPos.y)), minDefectPos, CV_RGB( 2500, 0, 0 ), 1, CV_AA, 0 );
			
			cvReleaseMemStorage( &storage );
			cvReleaseMemStorage( &storage1 );
			cvReleaseMemStorage( &storage2 );
			cvReleaseMemStorage( &storage3 );
			//return 0;
		}
	}
}



double rangosLongitudes [20][2] =
	{
		{62.4868, 82.4998}, //A
		{113.35, 192.293}, //C
		{80.9532, 123.694}, //E		
		{124.458, 157.178}, //U		
		{149.073, 185.217}, //1
		
		{111.876, 270.255}, //B
		{297.609, 361.505}, //I
		{289.027, 367.171}, //R
		
		{269.164, 409.923}, //D
		{219.531, 318.41}, //F
		{422.244, 578.86}, //G
		{491.106, 577.238}, //L
		{485.78, 552.635}, //P
		{422.017, 469.986}, //V, 2
		{419.554, 560.382}, //W

		{458.719, 653.324}, //H		
		{571.653, 647.173}, //4

		{705.86, 788.248}, //Y
		{660.127, 819.448}, //3

		{856.959, 952.936}, //5
	};

	double rangosRadio [20][2] =
	{
		{0.717557, 1.03252}, //A
		{0.548387, 1.83495}, //B
		{0.60733, 1.83495}, //C
		{0.564516, 1.16667}, //D
		{0.540373, 0.8}, //E
		{1.2037, 1.43571}, //F
		{0.693548, 1.10714}, //G
		{0.923387, 1.19903}, //H
		{0.810484, 0.931452}, //I
		{0.802419, 1}, //L
		{0.798387, 0.903226}, //P
		{0.765182, 1.01531}, //R
		{0.872727, 1.50725}, //U
		{0.80597, 1.00508}, //V, 2
		{0.810484, 1.38462}, //W
		{0.943548, 1.03766}, //Y
		{1.12, 1.66116}, //1
		{0.842742, 0.903226}, //3
		{0.864629, 1.1016}, //4
		{0.830645, 0.91129}, //5
	};



char identificaGesto (double longitud, int numDef, double radio)
{
	if (saveLength)
	{
		int l=5;
	}

	switch (numDef)
	{
	case 1: //0-4
		if (longitud > rangosLongitudes[0][0] && longitud < rangosLongitudes[0][1])
			return 'a';		
		if (longitud > rangosLongitudes[2][0] && longitud < rangosLongitudes[2][1])
			return 'e';
		if (longitud > rangosLongitudes[3][0] && longitud < rangosLongitudes[3][1])
			return 'u';
		if (longitud > rangosLongitudes[4][0] && longitud < rangosLongitudes[4][1])
			return '1';
		if (longitud > rangosLongitudes[1][0] && longitud < rangosLongitudes[1][1])
			return 'c';
		break;
	case 2://5-7
		if (longitud > rangosLongitudes[5][0] && longitud < rangosLongitudes[5][1])
			return 'b';
		if (longitud > rangosLongitudes[7][0] && longitud < rangosLongitudes[7][1])
			return 'r';
		if (longitud > rangosLongitudes[6][0] && longitud < rangosLongitudes[6][1])
			return 'i';
		break;
	case 3://8-14			
		if (longitud > rangosLongitudes[9][0] && longitud < rangosLongitudes[9][1])
			return 'f';
		if (longitud > rangosLongitudes[8][0] && longitud < rangosLongitudes[8][1])
			return 'd';		
		if (longitud > rangosLongitudes[14][0] && longitud < rangosLongitudes[14][1])
			return 'w';
		if (longitud > rangosLongitudes[13][0] && longitud < rangosLongitudes[13][1])
			return 'v';
		if (longitud > rangosLongitudes[10][0] && longitud < rangosLongitudes[10][1])
			return 'g';
		if (longitud > rangosLongitudes[12][0] && longitud < rangosLongitudes[12][1])
			return 'p';		
		if (longitud > rangosLongitudes[11][0] && longitud < rangosLongitudes[11][1])
			return 'l';
		break;
	case 4://15,16
		if (longitud > rangosLongitudes[15][0] && longitud < rangosLongitudes[15][1])
			return 'h';
		if (longitud > rangosLongitudes[16][0] && longitud < rangosLongitudes[16][1])
			return '4';
		break;
	case 5://17,18
		if (longitud > rangosLongitudes[17][0] && longitud < rangosLongitudes[17][1])
			return 'y';
		if (longitud > rangosLongitudes[18][0] && longitud < rangosLongitudes[18][1])
			return '3';
		break;
	case 6://19;
		if (longitud > rangosLongitudes[19][0] && longitud < rangosLongitudes[19][1])
			return '5';
		break;
	}

	return ' ';
}



double getMin (double a, double b, double c, double d)
{
	using std::min;
	
	return min (a, (min (b, min (c,d))));
}

double getMax (double a, double b, double c, double d)
{
	using std::max;
	
	return max (a, (max (b, max (c,d))));
}

double longBtwnPoints (CvPoint a, CvPoint b)
{
	double x = (a.x - b.x);
	x = x*x;
	double y = (a.y - b.y);
	y = y*y;
	return sqrt (x + y);	
}