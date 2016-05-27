/*
 * coiltest.cpp
 *
 *  Created on: Jan 25, 2012
 *      Author: jqueisse
 */

#include <stdlib.h>
#include <time.h>
#include <iostream>
#include <string>
#include <sstream>

#include <opencv/cv.h>
#include <opencv/highgui.h>

#ifndef COILDATABASE
#define COILDATABASE "/hri/localdisk/vlosing/coil100/coil-100/obj"
#endif


extern"C"
{
#include "Network_c.h"
#include "NetworkSerialization.h"
}


int main_normal()
{
	srand ( 0 );
	std::cout << "GRLVQ - Coil 100 test !\n";

	int numofobjects=10;
	int objstepsize=10;
	int numofviews=10;
	int viewstepsize=20;

	int numofthreads=1;

	PROTOFRMT* traindata=NULL;
	PROTOFRMT* evaldata=NULL;

	unsigned int dimensionality=0;





	for (int obj=0; obj<numofobjects; obj++)
	{
		int currobj = obj*objstepsize+1;

		for (int view=0; view<numofviews; view++)
		{
			int currview = view*viewstepsize;

			std::stringstream path;
			path << COILDATABASE;
			path << currobj << "__" << currview << ".png";

			std::stringstream path2;
			path2 << COILDATABASE;
			path2 << currobj << "__" << currview+viewstepsize/2 << ".png";

			std::string img = path.str();
			std::string img2 = path2.str();

			std::cout << "Loading: " << img << "\n";

			cv::Mat imgdata = cv::imread(img, CV_LOAD_IMAGE_GRAYSCALE);
			cv::Mat scaledimg;
			cv::resize(imgdata,scaledimg, cv::Size(), 0.1, 0.1 );

			cv::Mat imgdata2 = cv::imread(img2, CV_LOAD_IMAGE_GRAYSCALE);
			cv::Mat scaledimg2;
			cv::resize(imgdata2, scaledimg2, cv::Size(), 0.1, 0.1 );

			std::cout << "matdims: " << scaledimg2.size().height << " channels: "<< scaledimg2.size().width << "\n";

			cv::namedWindow("imgprev");
			cv::imshow("imgprev", imgdata);


			dimensionality = (unsigned int)scaledimg.rows*(unsigned int)scaledimg.cols;

			if (traindata==NULL)
			{
				traindata = new PROTOFRMT[dimensionality * numofobjects * numofviews];
				evaldata = new PROTOFRMT[dimensionality * numofobjects * numofviews];
			}



			int pos= obj*numofviews*dimensionality+view*dimensionality;
			for(int row = 0; row < scaledimg.rows; ++row)
			{
			    unsigned char* inp  = scaledimg.ptr<unsigned char>(row);
			    for (int col = 0; col < scaledimg.cols; ++col)
			    {
			    	unsigned char val = *inp++;
			    	traindata[pos]=val/250.0;
			    	pos++;
			    }
			    //traindata[pos++]=obj/(double)numofobjects;
			}

			pos= obj*numofviews*dimensionality+view*dimensionality;
			for(int row = 0; row < scaledimg2.rows; ++row)
			{
			    unsigned char* inp  = scaledimg2.ptr<unsigned char>(row);
			    for (int col = 0; col < scaledimg2.cols; ++col)
			    {
			    	unsigned char val = *inp++;
			    	evaldata[pos]=val/250.0;
			    	pos++;
			    }
			    //evaldata[pos++]=obj/(double)numofobjects;
			}

			cv::waitKey(50);
		}
	}




	//Train:
	int numofprototypes=4;
	int trainsteps=1000;
	bool learnrate_per_node=true;

	Network_c* grlvqnet = network_create(GMLVQ, dimensionality,learnrate_per_node, numofprototypes*numofobjects, trainsteps, numofthreads);



	for (int obj=0; obj<numofobjects; obj++)
	{
		for (int proto=0; proto<numofprototypes; proto++)
		{
			network_addPrototype(grlvqnet, obj, NULL);
		}
	}


	for (int e=0; e<trainsteps; e++)
	{
		//std::cout << "Training Sample " << e << "\n";
		int objnr = rand() % numofobjects;
		int viewnr = rand() % numofviews;
		int pos = objnr*numofviews*dimensionality+viewnr*dimensionality;

		int label=objnr;
		PROTOFRMT* data=&(traindata[pos]);

		//int mode = NETMODE_WEIGHTS;
		//if ((e%100)>50) mode = NETMODE_LAMBDA;

		PROTOFRMT diff = network_trainStep(grlvqnet, data, &label, 1, true);
		printf ("deltadist %f\n", diff);
	}



	//Evaluate result:
	std::cout << "Start Evaluation\n";
	int correctcount=0;
	int samplecount=0;
	int correctclasscount[numofobjects];
	for (int obj=0; obj<numofobjects; obj++)
	{
		correctclasscount[obj]=0;

		for (int view=0; view<numofviews; view++)
		{
			int pos = obj*numofviews*dimensionality+view*dimensionality;
			int label=obj;
			PROTOFRMT* data=&(evaldata[pos]);

			int resultlbl;
			network_processData(grlvqnet, data, &resultlbl, 1, NULL);

			if (resultlbl==label)
			{
				//correct!
				correctcount++;
				correctclasscount[obj]++;
			}
			else
			{

			}
			samplecount++;
		}

		std::cout << "Class " << obj << " detection rate : " << 100.0*correctclasscount[obj]/(double)numofviews << "\%\n";
	}

	std::cout << "Overall detection rate : " << 100.0*correctcount/(double)samplecount << "\%(" << samplecount <<" samples)\n";

	network_printMetricWeights(grlvqnet);






	delete [] traindata;
	delete [] evaldata;



	return 0;
}

int main_incremental()
{
	srand ( 0 );
	std::cout << "GRLVQ - Coil 100 test !\n";

	int numofobjects=10;
	int objstepsize=10;
	int numofviews=10;
	int viewstepsize=20;

	int numofthreads=1;

	PROTOFRMT* traindata=NULL;
	int* trainlabel=NULL;
	PROTOFRMT* evaldata=NULL;

	unsigned int dimensionality=0;




	int pos_train=0;
	int pos_eval=0;
	int pos=0;
	for (int obj=0; obj<numofobjects; obj++)
	{
		int currobj = obj*objstepsize+1;

		for (int view=0; view<numofviews; view++)
		{
			int currview = view*viewstepsize;

			std::stringstream path;
			path << COILDATABASE;
			path << currobj << "__" << currview << ".png";

			std::stringstream path2;
			path2 << COILDATABASE;
			path2 << currobj << "__" << currview+viewstepsize/2 << ".png";

			std::string img = path.str();
			std::string img2 = path2.str();

			std::cout << "Loading: " << img << "\n";


			cv::Mat imgdata = cv::imread(img, CV_LOAD_IMAGE_GRAYSCALE);
			cv::Mat scaledimg;
			cv::resize(imgdata,scaledimg, cv::Size(), 0.1, 0.1 );
			cv::Mat imgdata2 = cv::imread(img2, CV_LOAD_IMAGE_GRAYSCALE);
			cv::Mat scaledimg2;
			cv::resize(imgdata2, scaledimg2, cv::Size(), 0.1, 0.1 );

			//std::cout << "matdims: " << imgdata.dims << " channels: "<< imgdata.channels() << "\n";

			cv::namedWindow("imgprev");
			cv::imshow("imgprev", imgdata);


			dimensionality = (unsigned int)scaledimg.rows*(unsigned int)scaledimg.cols;

			if (traindata==NULL)
			{
				traindata = new PROTOFRMT[dimensionality * numofobjects * numofviews];
				trainlabel = new int[numofobjects * numofviews];
				evaldata = new PROTOFRMT[dimensionality * numofobjects * numofviews];
			}




			for(int row = 0; row < scaledimg.rows; ++row)
			{
			    unsigned char* inp  = scaledimg.ptr<unsigned char>(row);
			    for (int col = 0; col < scaledimg.cols; ++col)
			    {
			    	unsigned char val = *inp++;
			    	traindata[pos_train]=val/250.0;
			    	pos_train++;
			    }
			    //traindata[pos++]=obj/(double)numofobjects;
			}
			trainlabel[pos]=obj;
			pos++;

			for(int row = 0; row < scaledimg2.rows; ++row)
			{
			    unsigned char* inp  = scaledimg2.ptr<unsigned char>(row);
			    for (int col = 0; col < scaledimg2.cols; ++col)
			    {
			    	unsigned char val = *inp++;
			    	evaldata[pos_eval]=val/250.0;
			    	pos_eval++;
			    }
			    //evaldata[pos++]=obj/(double)numofobjects;
			}

			cv::waitKey(5);
		}
	}

	//Train:
	int numofprototypes=1;
	int trainepochs=30;
	bool learnrate_per_node=true;
	unsigned int g_max = 100;

	int incr_start=15;

	Network_c*  grlvqnet = network_create(GMLVQ, dimensionality,learnrate_per_node, numofprototypes*numofobjects, trainepochs*numofobjects*numofviews, numofthreads);


	for (int obj=0; obj<numofobjects; obj++)
	{
		for (int proto=0; proto<numofprototypes; proto++)
		{
			network_addPrototype(grlvqnet, obj, NULL);
		}
	}

	int relevance_start=10;
	std::cout << "start training\n";
	for (int e=0; e<trainepochs; e++)
	{
		bool adaptMetricWeights = e>=relevance_start;

		bool is_failure_increment=(e<=incr_start); // !!!!!!!!

		PROTOFRMT dist= network_trainStepIncremental(grlvqnet, traindata, trainlabel, is_failure_increment,  g_max, numofviews*numofobjects, true, NULL, adaptMetricWeights);

		std::cout << "Incremental Train Epoch " << e << " deltaweights:" << dist << " failure_increment: " << is_failure_increment << "\n";



	}



	//Evaluate result:
	std::cout << "Start Evaluation\n";
	int correctcount=0;
	int samplecount=0;
	int correctclasscount[numofobjects];
	for (int obj=0; obj<numofobjects; obj++)
	{
		correctclasscount[obj]=0;

		for (int view=0; view<numofviews; view++)
		{
			int pos = obj*numofviews*dimensionality+view*dimensionality;
			int label=obj;
			PROTOFRMT* data=&(evaldata[pos]);

			int resultlbl;
			network_processData(grlvqnet, data, &resultlbl, 1, NULL);

			std::cout << " eval : " << resultlbl << " -- " << label <<"\n";
			if (resultlbl==label)
			{
				//correct!
				correctcount++;
				correctclasscount[obj]++;
			}
			else
			{

			}
			samplecount++;
		}

		std::cout << "Class " << obj << " detection rate : " << 100.0*correctclasscount[obj]/(double)numofviews << "\%\n";
	}

	std::cout << "Overall detection rate: " << 100.0*correctcount/(double)samplecount << "\%(" << samplecount <<" samples)\n";

	network_printMetricWeights(grlvqnet);


	network_printStatus(grlvqnet);


	network_saveFile(grlvqnet, (char*)"testnet.net");
	network_printStatus(grlvqnet);





	Network_c* grlvqnet2 = network_createFile((char*)"testnet.net");
	std::cout << "Start Evaluation 2\n";

	correctcount=0;
	samplecount=0;

	for (int obj=0; obj<numofobjects; obj++)
	{
		correctclasscount[obj]=0;

		for (int view=0; view<numofviews; view++)
		{
			int pos = obj*numofviews*dimensionality+view*dimensionality;
			int label=obj;
			PROTOFRMT* data=&(evaldata[pos]);

			int resultlbl;
			network_processData(grlvqnet2, data, &resultlbl, 1, NULL);

			std::cout << " eval : " << resultlbl << " -- " << label <<"\n";
			if (resultlbl==label)
			{
				//correct!
				correctcount++;
				correctclasscount[obj]++;
			}
			else
			{

			}
			samplecount++;
		}

		std::cout << "Class " << obj << " detection rate : " << 100.0*correctclasscount[obj]/(double)numofviews << "\%\n";
	}

	std::cout << "Overall detection rate : " << 100.0*correctcount/(double)samplecount << "\%(" << samplecount <<" samples)\n";

	network_printMetricWeights(grlvqnet2);

	network_printStatus(grlvqnet2);


	//Visualize Lambdas:
	PROTOFRMT *lambdadata=new PROTOFRMT[network_getMetricWeightsLen(grlvqnet2)];
	network_getMetricWeights(grlvqnet2, 0,lambdadata);

	PROTOFRMT maxval=0.0;
	for (unsigned int k=0; k<dimensionality*dimensionality; k++)
	{
		if (maxval<lambdadata[k]) maxval=lambdadata[k];
	}

	for (unsigned int k=0; k<dimensionality*dimensionality; k++)
	{
		lambdadata[k]/=maxval;
	}


	cv::Mat lambdaimg(dimensionality,dimensionality, CV_64FC1, lambdadata);
	cv::namedWindow("Lambdas");
	cv::imshow("Lambdas", lambdaimg);
	cv::waitKey(0);
	delete []lambdadata;






	delete [] trainlabel;
	delete [] traindata;
	delete [] evaldata;



	return 0;
}


int main()
{
	return main_normal();
}
