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
controller * controllerPointer;
rcb * rcbPointer;
Urb * urbPointer;
perfect_link * plPointer;

std::thread * sender;
std::thread * listener;
std::thread * checker;
std::thread * extractorUrb;
std::thread * delivererUrb;
std::thread * extractorRcb;

static void stop(int) {
  // reset signal handlers to default
  signal(SIGTERM, SIG_DFL);
  signal(SIGINT, SIG_DFL);

// immediately stop network packet processing
  std::cout << "Immediately stopping network packet processing.\n";
  controllerPointer->stopThreads();

  listener->join();
  std::cout << "listener stopped" << std::endl;
  checker->join();
  std::cout << "checker stopped" << std::endl;
  extractorUrb->join();
  std::cout << "extractorUrb stopped" << std::endl;
  delivererUrb->join();
  std::cout << "delivererUrb stopped" << std::endl;
  extractorRcb->join();

  // write/flush output file if necessary
  std::cout << "Writing output.\n";
  controllerPointer->flushBuffer();

  // free network resources
  plPointer->free_network();

  delete plPointer;
  delete urbPointer;
  delete rcbPointer;
  delete controllerPointer;

  delete listener;
  delete checker;
  delete extractorUrb;
  delete delivererUrb;
  delete extractorRcb;
  delete sender;

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

    // This was the used one
    plPointer->addHost(host.id, host.portReadable(), host.ipReadable());

    if (host.id == parser.id()) {
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

  // open config file and read value
  // create data structures
  // initialize network
  // do everything I can to save time

  urbPointer = new Urb(plPointer);
  rcbPointer = new rcb(urbPointer);
  controllerPointer = new controller(parser.id(), parser.configPath(), parser.outputPath(), rcbPointer);
  
  listener = new std::thread(&perfect_link::handleMessages, plPointer);

  std::cout << "Waiting for all processes to finish initialization\n\n";
  coordinator.waitOnBarrier();

  std::cout << "Broadcasting messages...\n\n";

  sender = new std::thread(&controller::broadcast, controllerPointer);

  checker = new std::thread(&perfect_link::checker, plPointer);
  extractorUrb = new std::thread(&Urb::extractFromDelivering, urbPointer);
  delivererUrb = new std::thread(&Urb::checkToDeliver, urbPointer);
  extractorRcb = new std::thread(&rcb::extractFromDelivering, rcbPointer);

  sender->join();

  std::cout << "Signaling end of broadcasting messages\n\n";
  coordinator.finishedBroadcasting();


  while (true) {
    std::this_thread::sleep_for(std::chrono::seconds(60));
  }

  return 0;
}
