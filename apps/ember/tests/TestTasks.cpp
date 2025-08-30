#include <cppunit/extensions/TestFactoryRegistry.h>
#include <cppunit/ui/text/TestRunner.h>
#include <cppunit/extensions/HelperMacros.h>
#include <cppunit/BriefTestProgressListener.h>
#include <cppunit/TestResult.h>

#include "framework/tasks/TaskQueue.h"
#include "framework/tasks/ITask.h"
#include "framework/tasks/ITaskExecutionListener.h"
#include "framework/tasks/TaskExecutionContext.h"
#include "framework/Exception.h"

#include <Eris/EventService.h>

#include <wfmath/timestamp.h>

#include <boost/asio.hpp>

#define _GLIBCXX_USE_NANOSLEEP 1

#include <thread>
#include <chrono>
#include <condition_variable>
#include <stdexcept>

namespace Ember {

class CounterTask : public Tasks::ITask {
public:

	int& mCounter;

	int mSleep;

	CounterTask(int& counter, int sleep = 0) :
			mCounter(counter), mSleep(sleep) {
		mCounter += 2;
	}

	void executeTaskInBackgroundThread(Tasks::TaskExecutionContext& context) override {
		if (mSleep) {
			std::this_thread::sleep_for(std::chrono::milliseconds(mSleep));
		}
		mCounter--;
	}

	/**
	 * @brief Executes the task in the main thread, after executeTaskInBackgroundThread() has been called.
	 * Since this will happen in the main thread you shouldn't do any time consuming things here, since it will lock up the rendering.
	 */
	bool executeTaskInMainThread() override {
		mCounter--;
		return true;
	}

	std::string getName() const override {
		return "CounterTask";
	}
};

struct TimeHolder {
public:
	WFMath::TimeStamp time;

};

struct TimeTask : public Tasks::ITask {

	TimeHolder& timeHolder;
	std::unique_ptr <ITask> subtask;
	Tasks::ITaskExecutionListener* listener;

	explicit TimeTask(TimeHolder& timeHolder, std::unique_ptr <ITask> subtask = {}, Tasks::ITaskExecutionListener* listener = nullptr)
			: timeHolder(timeHolder), subtask(std::move(subtask)), listener(listener) {
	}

	void executeTaskInBackgroundThread(Tasks::TaskExecutionContext& context) override {
		if (subtask) {
			//sleep a little so that we get different times on the tasks
			std::this_thread::sleep_for(std::chrono::milliseconds(5));
			context.executeTask(std::move(subtask), listener);
		}
		//sleep a little so that we get different times on the tasks
		std::this_thread::sleep_for(std::chrono::milliseconds(5));
	}

	bool executeTaskInMainThread() override {
		//sleep a little so that we get different times on the tasks
		std::this_thread::sleep_for(std::chrono::milliseconds(5));
		timeHolder.time = WFMath::TimeStamp::now();
		return true;
	}

	std::string getName() const override {
		return "TimeTask";
	}
};

class CounterTaskBackgroundException : public CounterTask {
public:
        CounterTaskBackgroundException(int& counter) : CounterTask(counter) {
        }

        virtual void executeTaskInBackgroundThread(Tasks::TaskExecutionContext& context) {
                throw Ember::Exception("CounterTaskBackgroundException");
        }
};

class CounterTaskBackgroundStdException : public CounterTask {
public:
        CounterTaskBackgroundStdException(int& counter) : CounterTask(counter) {
        }

        virtual void executeTaskInBackgroundThread(Tasks::TaskExecutionContext& context) {
                throw std::runtime_error("std::exception message");
        }
};

class SimpleListener : public Tasks::ITaskExecutionListener {
public:

        bool started;
        bool ended;
        bool error;
        WFMath::TimeStamp startedTime;
        WFMath::TimeStamp endedTime;
        WFMath::TimeStamp errorTime;
        std::string errorMessage;

        SimpleListener() : started(false), ended(false), error(false) {

        }

        virtual void executionStarted() {
                startedTime = WFMath::TimeStamp::now();
                started = true;
        }

        virtual void executionEnded() {
                endedTime = WFMath::TimeStamp::now();
                ended = true;
        }

        virtual void executionError(const Ember::Exception& exception) {
                errorTime = WFMath::TimeStamp::now();
                error = true;
                errorMessage = exception.what();
        }
};

class TaskTestCase : public CppUnit::TestFixture {
	CPPUNIT_TEST_SUITE(TaskTestCase);
	CPPUNIT_TEST(testSimpleTaskRun);
	CPPUNIT_TEST(testSimpleTaskRunAndPoll);
	CPPUNIT_TEST(testSimpleTaskRunTwoExecs);
	CPPUNIT_TEST(testSimpleTaskRunTwoTasks);
	CPPUNIT_TEST(testSimpleTaskRunTwoTasksTwoExecs);
	CPPUNIT_TEST(testListener);
        CPPUNIT_TEST(testBackgroundException);
        CPPUNIT_TEST(testBackgroundExceptionMessage);
	CPPUNIT_TEST(testTaskOrder);
	CPPUNIT_TEST(testSubTaskOrder);

	CPPUNIT_TEST_SUITE_END();

	boost::asio::io_context io_service;
public:
	void testSimpleTaskRun() {
		int counter = 0;
		{
			Eris::EventService es(io_service);
			Tasks::TaskQueue taskQueue(1, es);
			taskQueue.enqueueTask(std::make_unique<CounterTask>(counter));
		}
		CPPUNIT_ASSERT(counter == 0);
	}

	void testSimpleTaskRunAndPoll() {
		int counter = 0;
		{
			Eris::EventService es(io_service);
			Tasks::TaskQueue taskQueue(1, es);
			taskQueue.enqueueTask(std::make_unique<CounterTask>(counter));
			//200 ms should be enough... This isn't deterministic though.
			std::this_thread::sleep_for(std::chrono::milliseconds(200));
			es.processAllHandlers();
			CPPUNIT_ASSERT(counter == 0);
		}

	}

	void testSimpleTaskRunTwoExecs() {
		int counter = 0;
		{
			Eris::EventService es(io_service);
			Tasks::TaskQueue taskQueue(2, es);
			taskQueue.enqueueTask(std::make_unique<CounterTask>(counter));
		}
		CPPUNIT_ASSERT(counter == 0);
	}

	void testSimpleTaskRunTwoTasks() {
		int counter = 0;
		{
			Eris::EventService es(io_service);
			Tasks::TaskQueue taskQueue(1, es);
			taskQueue.enqueueTask(std::make_unique<CounterTask>(counter, 200));
			taskQueue.enqueueTask(std::make_unique<CounterTask>(counter));
		}
		CPPUNIT_ASSERT(counter == 0);
	}

	void testSimpleTaskRunTwoTasksTwoExecs() {
		int counter = 0;
		{
			Eris::EventService es(io_service);
			Tasks::TaskQueue taskQueue(2, es);
			taskQueue.enqueueTask(std::make_unique<CounterTask>(counter, 200));
			taskQueue.enqueueTask(std::make_unique<CounterTask>(counter));
		}
		CPPUNIT_ASSERT(counter == 0);
	}

	void testListener() {
		SimpleListener listener;
		int counter = 0;
		{
			Eris::EventService es(io_service);
			Tasks::TaskQueue taskQueue(1, es);
			taskQueue.enqueueTask(std::make_unique<CounterTask>(counter), &listener);
		}
		CPPUNIT_ASSERT(counter == 0);
		CPPUNIT_ASSERT(listener.started);
		CPPUNIT_ASSERT(listener.ended);
	}

        void testBackgroundException() {
                SimpleListener listener;
                int counter = 0;
                {
                        Eris::EventService es(io_service);
                        Tasks::TaskQueue taskQueue(1, es);
                        taskQueue.enqueueTask(std::make_unique<CounterTaskBackgroundException>(counter), &listener);
                }
                CPPUNIT_ASSERT(counter == 1);
                CPPUNIT_ASSERT(listener.error);
                CPPUNIT_ASSERT_EQUAL(std::string("CounterTaskBackgroundException"), listener.errorMessage);
        }

        void testBackgroundExceptionMessage() {
                SimpleListener listener;
                int counter = 0;
                {
                        Eris::EventService es(io_service);
                        Tasks::TaskQueue taskQueue(1, es);
                        taskQueue.enqueueTask(std::make_unique<CounterTaskBackgroundStdException>(counter), &listener);
                }
                CPPUNIT_ASSERT(listener.error);
                CPPUNIT_ASSERT_EQUAL(std::string("std::exception message"), listener.errorMessage);
        }

	void testTaskOrder() {
		SimpleListener listener1;
		SimpleListener listener2;
		SimpleListener listener3;
		TimeHolder time1;
		TimeHolder time2;
		TimeHolder time3;
		{
			Eris::EventService es(io_service);
			Tasks::TaskQueue taskQueue(1, es);
			taskQueue.enqueueTask(std::make_unique<TimeTask>(time1), &listener1);
			taskQueue.enqueueTask(std::make_unique<TimeTask>(time2), &listener2);
			taskQueue.enqueueTask(std::make_unique<TimeTask>(time3), &listener3);
		}
		CPPUNIT_ASSERT(listener1.startedTime < listener2.startedTime);
		CPPUNIT_ASSERT(listener2.startedTime < listener3.startedTime);
		CPPUNIT_ASSERT(listener1.endedTime < listener2.endedTime);
		CPPUNIT_ASSERT(listener2.endedTime < listener3.endedTime);
		CPPUNIT_ASSERT(time1.time < time2.time);
		CPPUNIT_ASSERT(time2.time < time3.time);
	}

	void testSubTaskOrder() {
		SimpleListener listener1;
		SimpleListener listener2;
		SimpleListener listener3;
		TimeHolder time1;
		TimeHolder time2;
		TimeHolder time3;
		{
			Eris::EventService es(io_service);
			Tasks::TaskQueue taskQueue(1, es);
			taskQueue.enqueueTask(std::make_unique<TimeTask>(time1, std::make_unique<TimeTask>(time2), &listener2), &listener1);
			taskQueue.enqueueTask(std::make_unique<TimeTask>(time3), &listener3);
		}
		//task 1 should start before task 2
		CPPUNIT_ASSERT(listener1.startedTime < listener2.startedTime);
		CPPUNIT_ASSERT(listener2.startedTime < listener3.startedTime);
		//However, task 2 should end before task 1
		CPPUNIT_ASSERT(listener2.endedTime < listener1.endedTime);
		CPPUNIT_ASSERT(listener1.endedTime < listener3.endedTime);
		//And task 2 should be run in the main thread before task 1
		CPPUNIT_ASSERT(time2.time < time1.time);
		CPPUNIT_ASSERT(time1.time < time3.time);
	}


};

}

CPPUNIT_TEST_SUITE_REGISTRATION(Ember::TaskTestCase);

int main(int argc, char** argv) {
	CppUnit::TextUi::TestRunner runner;
	CppUnit::TestFactoryRegistry& registry = CppUnit::TestFactoryRegistry::getRegistry();
	runner.addTest(registry.makeTest());

	// Shows a message as each test starts
	CppUnit::BriefTestProgressListener listener;
	runner.eventManager().addListener(&listener);

	bool wasSuccessful = runner.run("", false);
	return !wasSuccessful;
}

