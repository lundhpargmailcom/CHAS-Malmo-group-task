// test_store.cpp
#include "message_store.hpp"
#include <pthread.h>
#include <cstdio>
#include <cstring>

// Delad MessageStore-instans (skapas i main, skickas till trådar)
struct ThreadArgs
{
    MessageStore *store; // Referens via pekare
    int thread_id;
};


void *writer_thread(void *arg)
{
    ThreadArgs *args = static_cast<ThreadArgs *>(arg);
    char msg[256];
    for (int i = 0; i < 5; i++)
    {
        snprintf(msg, sizeof(msg), "Thread %d: message %d",
                 args->thread_id, i + 1);
        args->store->addMessage(msg);
        std::cout << "Sent: " << msg << std::endl;
    }

    // TODO: Hämta och skriv ut senaste meddelandet
    char latest[256];
    if (args->store->getLatestMessage(latest, sizeof(latest)))
    {
        std::cout << "Thread " << args->thread_id
                  << " latest: " << latest << std::endl;
    }
    return nullptr; // C++ nullptr istället för NULL
}


int main()
{
    // MessageStore skapas på stacken - konstruktorn körs
    MessageStore store;
    const int NUM_THREADS = 4;
    pthread_t threads[NUM_THREADS];
    ThreadArgs args[NUM_THREADS];

    // Skapa trådar
    for (int i = 0; i < NUM_THREADS; i++)
    {
        args[i].store = &store; // Alla delar samma instans
        args[i].thread_id = i + 1;
        pthread_create(&threads[i], nullptr,
                       writer_thread, &args[i]);
    }

    // Vänta på alla trådar
    for (int i = 0; i < NUM_THREADS; i++)
    {
        pthread_join(threads[i], nullptr);
    }

    // Skriv ut alla meddelanden
    store.printAll();
    std::cout << "Total amount: " << store.getMessageCount() << std::endl;
    
    // Destruktorn körs automatiskt när store går ur scope!
    return 0;
}