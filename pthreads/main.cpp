#include <assert.h>
#include <stdlib.h>
#include "ts_queue.hpp"
#include "item.hpp"
#include "reader.hpp"
#include "writer.hpp"
#include "producer.hpp"
#include "consumer_controller.hpp"

#define READER_QUEUE_SIZE 200
#define WORKER_QUEUE_SIZE 200
#define WRITER_QUEUE_SIZE 4000
#define CONSUMER_CONTROLLER_LOW_THRESHOLD_PERCENTAGE 20
#define CONSUMER_CONTROLLER_HIGH_THRESHOLD_PERCENTAGE 80
#define CONSUMER_CONTROLLER_CHECK_PERIOD 1000000

int main(int argc, char** argv) {
	assert(argc == 4);

	int n = atoi(argv[1]);
	std::string input_file_name(argv[2]);
	std::string output_file_name(argv[3]);

	// TODO: implements main function
	TSQueue<Item*> * inputQueue;
	TSQueue<Item*> * outputQueue;
	TSQueue<Item*> * workerQueue;

	inputQueue = new TSQueue<Item*>(READER_QUEUE_SIZE);
	outputQueue = new TSQueue<Item*>(WRITER_QUEUE_SIZE);
	workerQueue = new TSQueue<Item*>(WORKER_QUEUE_SIZE);

	Reader * rd = new Reader(n,input_file_name, inputQueue);
	Writer * wr = new Writer(n,output_file_name, outputQueue);

	Transformer *tr = new Transformer;
    
	ConsumerController * cc = new ConsumerController(workerQueue, outputQueue, tr, CONSUMER_CONTROLLER_CHECK_PERIOD, 
	WORKER_QUEUE_SIZE*CONSUMER_CONTROLLER_LOW_THRESHOLD_PERCENTAGE/100,
	WORKER_QUEUE_SIZE*CONSUMER_CONTROLLER_HIGH_THRESHOLD_PERCENTAGE/100);

	Producer * p1 = new Producer(inputQueue, workerQueue, tr);
	Producer * p2 = new Producer(inputQueue, workerQueue, tr);
	Producer * p3 = new Producer(inputQueue, workerQueue, tr);
	Producer * p4 = new Producer(inputQueue, workerQueue, tr);

	rd->start();
	wr->start();

	p1->start();
	p2->start();
	p3->start();
	p4->start();

	cc->start();

	rd->join();
	wr->join();


	delete rd;
	delete wr;
	delete p1;
	delete p2;
	delete p3;
	delete p4;
	//delete inputQueue;
	//delete outputQueue;
	//delete workerQueue;
	delete tr;
	delete cc;


	return 0;
}
