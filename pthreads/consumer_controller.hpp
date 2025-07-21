#include <pthread.h>
#include <unistd.h>
#include <vector>
#include <iostream>
#include "consumer.hpp"
#include "ts_queue.hpp"
#include "item.hpp"
#include "transformer.hpp"
#include <chrono>

#ifndef CONSUMER_CONTROLLER
#define CONSUMER_CONTROLLER

class ConsumerController : public Thread {
public:
	// constructor
	ConsumerController(
		TSQueue<Item*>* worker_queue,
		TSQueue<Item*>* writer_queue,
		Transformer* transformer,
		int check_period,
		int low_threshold,
		int high_threshold
	);

	// destructor
	~ConsumerController();

	virtual void start();

private:
	std::vector<Consumer*> consumers;

	TSQueue<Item*>* worker_queue;
	TSQueue<Item*>* writer_queue;

	Transformer* transformer;

	// Check to scale down or scale up every check period in microseconds.
	int check_period;
	// When the number of items in the worker queue is lower than low_threshold,
	// the number of consumers scaled down by 1.
	int low_threshold;
	// When the number of items in the worker queue is higher than high_threshold,
	// the number of consumers scaled up by 1.
	int high_threshold;

	static void* process(void* arg);
};

// Implementation start

ConsumerController::ConsumerController(
	TSQueue<Item*>* worker_queue,
	TSQueue<Item*>* writer_queue,
	Transformer* transformer,
	int check_period,
	int low_threshold,
	int high_threshold
) : worker_queue(worker_queue),
	writer_queue(writer_queue),
	transformer(transformer),
	check_period(check_period),
	low_threshold(low_threshold),
	high_threshold(high_threshold) {
}

long long getCurrentTimeMicroseconds() {
    struct timespec currentTime;
    clock_gettime(CLOCK_REALTIME, &currentTime);  // or use CLOCK_MONOTONIC

    // Convert to microseconds
    return currentTime.tv_sec * 1000000LL + currentTime.tv_nsec / 1000;
}
ConsumerController::~ConsumerController() {}

void ConsumerController::start() {
	// TODO: starts a ConsumerController thread
	pthread_create(&t, 0, ConsumerController::process, (void*)this);
}

void* ConsumerController::process(void* arg) {
	// TODO: implements the ConsumerController's work
	//auto timestamp = std::chrono::high_resolution_clock::now();
	//assign time to a variable no chrono
	long long timestamp = getCurrentTimeMicroseconds();
	ConsumerController* cc = (ConsumerController*)arg;

	while(true){
		long long timestamp2 = getCurrentTimeMicroseconds();
		double elapsed_time = static_cast<double>(timestamp2 - timestamp);
		//std::cout << "worker queue size " << cc->worker_queue->get_size() << std::endl;
		if(elapsed_time > cc->check_period){
			int worker_queue_size = cc->worker_queue->get_size();
			//std:: cout << "worker queue size " << worker_queue_size << std::endl;
			if(worker_queue_size > cc->high_threshold){
				std::cout<< "scale up consumer from "<< cc->consumers.size() <<
				" to "<<cc->consumers.size() + 1<< std::endl;
				Consumer* new_consumer = new Consumer(cc->worker_queue, cc->writer_queue, cc->transformer);
				new_consumer->start();
				cc->consumers.push_back(new_consumer);
			}
			else if(worker_queue_size < cc->low_threshold && cc->consumers.size() > 1){
				std::cout<< "scale down consumer from "<< cc->consumers.size() <<
				" to "<<cc->consumers.size() - 1<< std::endl;
				cc->consumers.back()->cancel();
				cc->consumers.pop_back();
			}
			timestamp = getCurrentTimeMicroseconds();
		}
	}
}

#endif // CONSUMER_CONTROLLER_HPP
