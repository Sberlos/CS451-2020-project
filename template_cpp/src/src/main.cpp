#include <chrono>
#include <iostream>
#include <thread>

#include "barrier.hpp"
#include "parser.hpp"
#include "host.hpp"
#include "hello.h"
#include <signal.h>
#include <controller.hpp>
#include <rcb.hpp>
#include <urb.hpp>
#include <perfectLink.hpp>

// terrible thing having a global but I don't know how to avoid it
//HostC * hostRef;
controller * controllerPointer;
rcb * rcbPointer;
Urb * urbPointer;
perfect_link * plPointer;
std::atomic_bool run = true;

std::thread * listenerTPointer;
std::thread * checkerTPointer;
std::thread * extractorUrbTPointer;
std::thread * delivererUrbTPointer;
std::thread * extractorRcbTPointer;

static void stop(int) {
  // reset signal handlers to default
  signal(SIGTERM, SIG_DFL);
  signal(SIGINT, SIG_DFL);

// immediately stop network packet processing
  std::cout << "Immediately stopping network packet processing.\n";
  /*
  hostRef->stopThreads();

  // I would have to have a join here but not sure how to have the reference to the Threads
  std::this_thread::sleep_for(std::chrono::seconds(3));
  */
  urbPointer->stopThreads();
  std::this_thread::sleep_for(std::chrono::seconds(2));

  /*
  // let's try with join
  listenerTPointer->join();
  checkerTPointer->join();
  extractorUrbTPointer->join();
  delivererUrbTPointer->join();
  extractorRcbTPointer->join();
  */

  // write/flush output file if necessary
  std::cout << "Writing output.\n";
  controllerPointer->flushBuffer();

  // free network resources
  // I think I can do it in the initialization method
  plPointer->free_network();

  delete plPointer;
  delete urbPointer;
  delete rcbPointer;
  delete controllerPointer;

  // exit directly from signal handler
  exit(0);
}

int main(int argc, char **argv) {
  signal(SIGTERM, stop);
  signal(SIGINT, stop);

  // `true` means that a config file is required.
  // Call with `false` if no config file is necessary.
  bool requireConfig = true;

  Parser parser(argc, argv, requireConfig);
  parser.parse();

  hello();

  /*
  HostC node = HostC(parser.id(), parser.configPath(), parser.outputPath());
  hostRef = &node;
  */
  /* This was the used one
  HostC *node = new HostC(parser.id(), parser.configPath(), parser.outputPath());
  hostRef = node;
  */
  plPointer = new perfect_link(parser.id());

  std::cout << std::endl;

  std::cout << "My PID: " << getpid() << "\n";
  std::cout << "Use `kill -SIGINT " << getpid() << "` or `kill -SIGTERM "
            << getpid() << "` to stop processing packets\n\n";

  std::cout << "My ID: " << parser.id() << "\n\n";

  std::cout << "Path to hosts:\n";
  std::cout << "==============\n";
  std::cout << parser.hostsPath() << "\n\n";

  std::cout << "List of resolved hosts is:\n";
  std::cout << "==========================\n";
  auto hosts = parser.hosts();
  for (auto &host : hosts) {

    //node.addHost(host.id, host.port, host.ip);
    //node.addHost2(host.id, host.portReadable(), host.ipReadable());

    // This was the used one
    plPointer->addHost(host.id, host.portReadable(), host.ipReadable());

    if (host.id == parser.id()) {
      //node.initialize_network2(host.portReadable());
      plPointer->initialize_network(host.portReadable());
    }

    std::cout << host.id << "\n";
    std::cout << "Human-readable IP: " << host.ipReadable() << "\n";
    std::cout << "Machine-readable IP: " << host.ip << "\n";
    std::cout << "Human-readbale Port: " << host.portReadable() << "\n";
    std::cout << "Machine-readbale Port: " << host.port << "\n";
    std::cout << "\n";
  }
  std::cout << "\n";

  std::cout << "Barrier:\n";
  std::cout << "========\n";
  auto barrier = parser.barrier();
  std::cout << "Human-readable IP: " << barrier.ipReadable() << "\n";
  std::cout << "Machine-readable IP: " << barrier.ip << "\n";
  std::cout << "Human-readbale Port: " << barrier.portReadable() << "\n";
  std::cout << "Machine-readbale Port: " << barrier.port << "\n";
  std::cout << "\n";

  std::cout << "Signal:\n";
  std::cout << "========\n";
  auto signal = parser.signal();
  std::cout << "Human-readable IP: " << signal.ipReadable() << "\n";
  std::cout << "Machine-readable IP: " << signal.ip << "\n";
  std::cout << "Human-readbale Port: " << signal.portReadable() << "\n";
  std::cout << "Machine-readbale Port: " << signal.port << "\n";
  std::cout << "\n";

  std::cout << "Path to output:\n";
  std::cout << "===============\n";
  std::cout << parser.outputPath() << "\n\n";

  if (requireConfig) {
    std::cout << "Path to config:\n";
    std::cout << "===============\n";
    std::cout << parser.configPath() << "\n\n";
  }

  std::cout << "Doing some initialization...\n\n";

  Coordinator coordinator(parser.id(), barrier, signal);

  //Host h = Host(atoi(parser.id().c_str()), parser

  // open config file and read value
  // create data structures
  // initialize network
  // do everything I can to save time

  urbPointer = new Urb(plPointer);
  rcbPointer = new rcb(urbPointer);
  controllerPointer = new controller(parser.id(), parser.configPath(), parser.outputPath(), rcbPointer);
  
  //node.initialize_network();
  //std::thread listener(&HostC::handleMessages, std::ref(node));
  std::thread listener(&perfect_link::handleMessages, plPointer);

  std::cout << "Waiting for all processes to finish initialization\n\n";
  coordinator.waitOnBarrier();

  std::cout << "Broadcasting messages...\n\n";
  /*
  std::thread sender(&HostC::startBroadcasting, node);

  */
  /*
  urbPointer->urbBroadcast(1);
  urbPointer->urbBroadcast(2);
  urbPointer->urbBroadcast(3);
  */
  /*
  rcbPointer->rcoBroadcast(1);
  rcbPointer->rcoBroadcast(2);
  rcbPointer->rcoBroadcast(3);
  */
  //controllerPointer->broadcast();
  std::thread sender(&controller::broadcast, controllerPointer);

  // Start to check only after the sender has broadcasted all the messages
  // This is for two reasons:
  // 1) I am already sending at max rate, no sense to add others things
  // 2) Doesn't make much sense to check for missing packets while I have not yet sent
  // everything

  std::thread checker(&perfect_link::checker, plPointer);
  std::thread extractorUrb(&Urb::extractFromDelivering, urbPointer);
  std::thread delivererUrb(&Urb::checkToDeliver, urbPointer);
  std::thread extractorRcb(&rcb::extractFromDelivering, rcbPointer);
  std::thread * listenerTPointer = &listener;
  std::thread * checkerTPointer = &checker;
  std::thread * extractorUrbTPointer = &extractorUrb;
  std::thread * delivererUrbTPointer = &delivererUrb;
  std::thread * extractorRcbTPointer = &extractorRcb;

  sender.join();

  std::cout << "Signaling end of broadcasting messages\n\n";
  coordinator.finishedBroadcasting();


  while (true) {
    std::this_thread::sleep_for(std::chrono::seconds(60));
  }

  return 0;
}
