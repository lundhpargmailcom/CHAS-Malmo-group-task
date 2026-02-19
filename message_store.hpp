// message_store.hpp
#ifndef MESSAGE_STORE_HPP
#define MESSAGE_STORE_HPP

#include <iostream>
#include <cstring>
#include <pthread.h>

class MessageStore
{
private:
    static const int MAX_MESSAGES = 100;
    static const int MAX_MSG_LEN = 256;
    char messages[MAX_MESSAGES][MAX_MSG_LEN];

    int count;
    int next_index;
    mutable pthread_mutex_t lock;

public:
    // TODO: Implementera konstruktorn
    // Ska initialisera count och next_index till 0
    // Ska initialisera mutex med pthread_mutex_init()
    MessageStore() : count(0), next_index(0)
    {
        // Din kod här
        pthread_mutex_init(&lock, nullptr);
        memset(messages, 0, sizeof(messages));
    }

    // TODO: Implementera destruktorn
    // Ska förstöra mutex med pthread_mutex_destroy()
    ~MessageStore()
    {
        // Din kod här
        pthread_mutex_destroy(&lock);
        std::cout << "MessageStore destroyed, had " << count << " messages" << std::endl;
    }

    // Förhindra kopiering (mutex kan inte kopieras)
    MessageStore(const MessageStore &) = delete;
    MessageStore &operator=(const MessageStore &) = delete;

    // TODO: Implementera addMessage()
    // Samma logik som C-versionen av add_message()
    // men nu som en medlemsfunktion
    void addMessage(const char *msg)
    {
        pthread_mutex_lock(&lock);

        // Din kod här - kopiera msg till rätt plats
        strncpy(messages[next_index], msg, MAX_MSG_LEN - 1);
        messages[next_index][MAX_MSG_LEN - 1] = '\0';

        // Uppdatera next_index och count
        next_index = (next_index + 1) % MAX_MESSAGES;
        if (count < MAX_MESSAGES)
            count++;
            
        pthread_mutex_unlock(&lock);
    }

    // TODO: Implementera getLatestMessage()
    // Returnera true om det finns ett meddelande, annars false
    bool getLatestMessage(char *buf, int buf_size)
    {
        pthread_mutex_lock(&lock);
        if (count == 0)
        {
            pthread_mutex_unlock(&lock);
            return false;
        }

        // Din kod här - kopiera senaste meddelandet till buf
        int latest_message = (next_index - 1 + MAX_MESSAGES) % MAX_MESSAGES;
        strncpy(buf, messages[latest_message], buf_size - 1);
        buf[buf_size - 1] = '\0';

        pthread_mutex_unlock(&lock);
        
        return true;
    }

    // TODO: Implementera getMessageCount()
    // Trådsäker hämtning av antal meddelanden
    int getMessageCount() const
    {
        // OBS: const-metod men vi behöver låsa mutex.
        pthread_mutex_lock(&lock);

        int c = count;
        // Vi kan använda const_cast här eller göra lock mutable.
        // Diskutera i gruppen: varför är detta problematiskt?
        pthread_mutex_unlock(&lock);

        return c;
    }

    // Skriv ut alla meddelanden (för debugging)
    void printAll() const
    {
        pthread_mutex_lock(&lock);

        std::cout << "=== Meddelanden (" << count
                  << " st) ===" << std::endl;
        int start = (count < MAX_MESSAGES) ? 0
                                           : next_index;
        int total = (count < MAX_MESSAGES) ? count
                                           : MAX_MESSAGES;
        for (int i = 0; i < total; i++)
        {
            int idx = (start + i) % MAX_MESSAGES;
            std::cout << "  [" << i + 1 << "] "
                      << messages[idx] << std::endl;
        }
        pthread_mutex_unlock(&lock);

    }
};

#endif // MESSAGE_STORE_HPP