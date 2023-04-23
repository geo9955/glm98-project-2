//Build command: g++ -o glm98-project-2.exe glm98-project-2.cpp -l pthread -std=c++11

#include <algorithm>
#include <deque>
#include <fstream>
#include <iostream>
#include <pthread.h>
#include <random>
#include <string>
#include <unistd.h>

using namespace std;


int seed,
    winner = -1,
    turn = -1,
    roundNum = 0;
const int NUM_PLAYERS = 6;
bool roundStart = false;


ofstream file("output.txt");


deque<string> deck;
deque<string> hands[NUM_PLAYERS];


mt19937* rng;
uniform_int_distribution<int> distribution(0,1);


pthread_t threads[NUM_PLAYERS];

pthread_mutex_t mutex;

pthread_mutex_t roundStartMutex;
pthread_cond_t roundStartCond;

pthread_mutex_t deckFullMutex;
pthread_cond_t deckFullCond;


void printCardsToStream(deque<string>*, ostream*);
void drawCard(int, bool);
void discard(int, bool);
bool checkForWin(int);
void *Player(void *);


int main(int argc, char **argv)
{ 
    //Initial error checking
    if(argc == 1) {
        cout << "Parameter usage: (RNG seed: int)" << endl;
        cout << "Example command:\n./glm98-project-2.exe 100" << endl;
        return 0;
    }

    if(argc > 2) {
        cout << "Incorrect number of parameters. Please run this program with no arguments to see valid parameter usage." << endl;
        return -1;
    }

    try {
        seed = stoi(argv[1]);
    } 
    catch(const exception &e) {
        cout << "Invalid argument. Seed must be an integer." << endl;
        return -1;
    }
    
    if(!file.is_open()) {
        cout << "Error opening output file. Terminating program.";
        return -1;
    }

    //Deck setup
    string values[] = {"A", "J", "Q", "K", "2", "3", "4", "5", "6", "7", "8", "9", "10"};

    for(string x : values) {
        for(int i = 0; i < 4; i++) {
            deck.push_back(x);
        }
    }

    //Object initializing
    rng = new mt19937(seed);

    mutex = PTHREAD_MUTEX_INITIALIZER;

    roundStartMutex = PTHREAD_MUTEX_INITIALIZER;
    roundStartCond = PTHREAD_COND_INITIALIZER;

    deckFullMutex = PTHREAD_MUTEX_INITIALIZER;
    deckFullCond = PTHREAD_COND_INITIALIZER;

    //Thread creation
    pthread_mutex_lock(&mutex);

    int rc;
    void *status;

    for(long i = 0; i <NUM_PLAYERS; i++) {
        cout << "In main: creating player number " << i + 1 << endl;
        rc = pthread_create(&threads[i], NULL, Player, (void *)i);
        if(rc) {
            cout << "Error; return code from pthread_create is " << rc << endl;
            return -1;
        }
    }

    pthread_mutex_unlock(&mutex);

    for(long i = 0; i < NUM_PLAYERS; i++) {
        rc = pthread_join(threads[i], &status);
        if (rc) {
            cout << "Error; return code from pthread_join is " << rc << endl;
            return -1;
        }
    }

    //Cleanup
    pthread_exit(NULL);
    file.close();
    delete rng;
    pthread_mutex_destroy(&mutex);
    pthread_mutex_destroy(&roundStartMutex);
    pthread_mutex_destroy(&deckFullMutex);
    pthread_cond_destroy(&roundStartCond);
    pthread_cond_destroy(&deckFullCond);
}

//Prints all cards in a deque (e.g. a hand or the whole deck) into the given output stream (e.g. cout or the output file)
void printCardsToStream(deque<string>* cards, ostream* stream) {
    for(string x : *cards)
        *stream << x << " ";
}

//Gives a player a card. If the second parameter is true, "deals" a player a card. Same thing, different word.
void drawCard(int id, bool dealing = false) {
    hands[id].push_back(deck.front());
    file << "PLAYER " << id + 1 << ": ";
    if(dealing)
        file << "Dealt ";
    else
        file << "Draws ";
        
    file << deck.front() << endl;
    deck.pop_front();
}

//Discards one card randomly, unless the second parameter is true, in which case all cards are returned
void discard(int id, bool discardAll = false) {
    file << "PLAYER " << id + 1 << ": ";

    if(discardAll) {
        while(hands[id].size() > 0) {
            deck.push_back(hands[id].front());
            hands[id].pop_front();
        }
        file << "Returns all cards to the deck." << endl;
        return;
    }

    file << "Discards ";
    int index = distribution(*rng);

    if(index == 0) {
        deck.push_back(hands[id].front());
        file << hands[id].front();
        hands[id].pop_front();
    }
    else {
        deck.push_back(hands[id].back());
        file << hands[id].back();
        hands[id].pop_back();
    }
    file << " at random." << endl;
}

//Checks whether a player has won. If they have, sets winner to their id and returns true
bool checkForWin(int id) {
    for(string x : hands[id]) {
        //Match found
        if(x.compare(hands[roundNum].front()) == 0) {
            winner = id;
            return true;
        }
    }

    return false;
}

void *Player(void *playerId) {
    long id;
    id = (long)playerId;

    while(roundNum < NUM_PLAYERS) {
        //Dealer pre-round
        if(id == roundNum) {
            pthread_mutex_lock(&mutex);

            winner = -1;

            //uses standard libary shuffle, most likely fisher-yates
            shuffle(deck.begin(), deck.end(), *rng);

            drawCard(id);

            cout << "PLAYER " << id + 1 << ": Target Card is " << hands[id].front() << endl;

            for(int i = 1; i < NUM_PLAYERS; i++) {
                file << "Dealing to player " << (((id + i)% NUM_PLAYERS) + 1) << endl;
                drawCard((id + i) % NUM_PLAYERS, true);

                if(checkForWin((id + i) % NUM_PLAYERS)) {
                    cout << "PLAYER " << ((id + i) % NUM_PLAYERS) + 1<< ": WIN - YES (Dealt a win)" << endl;
                    break;
                }
            }

            turn = id + 1;
            pthread_mutex_lock(&roundStartMutex);

            roundStart = true;
            pthread_cond_broadcast(&roundStartCond);

            pthread_mutex_unlock(&roundStartMutex);

            pthread_mutex_unlock(&mutex);
            sleep(1);
        }
        
        //Player pre-round
        else {
            pthread_mutex_lock(&roundStartMutex);

            while(!roundStart)
                pthread_cond_wait(&roundStartCond, &roundStartMutex);

            pthread_mutex_unlock(&roundStartMutex);
            sleep(1);
        }
        
        //While a winner hasn't been found
        while(winner < 0) {
            //If it's your turn...
            if(id == (turn % NUM_PLAYERS)) {
                //Dealer round action: just pass
                if(id == roundNum) {
                    pthread_mutex_lock(&mutex);

                    file << "Dealer passes their turn." << endl;
                    turn++;

                    pthread_mutex_unlock(&mutex);
                    sleep(1);
                }
                //Player round action
                else {
                    pthread_mutex_lock(&mutex);

                    file << "PLAYER " << id + 1 << ": Hand ";
                    printCardsToStream(&hands[id], &file);
                    file << endl;

                    drawCard(id);

                    cout << "PLAYER " << id + 1 << ": Hand ";
                    printCardsToStream(&hands[id], &cout);
                    cout << endl;
                    
                    //Win
                    if(checkForWin(id)) {
                        file << "PLAYER " << id + 1 << ": Hand " << " <> Target card is " << hands[roundNum].front() << endl;
                        cout << "PLAYER " << id + 1 << ": WIN - YES" << endl;
                    }

                    //No win
                    else {
                        cout << "PLAYER " << id + 1 << ": WIN - NO" << endl;

                        discard(id);
                        file << "PLAYER " << id + 1 << ": Hand ";
                        printCardsToStream(&hands[id], &file);

                        file << endl << "DECK: ";
                        printCardsToStream(&deck, &file);
                        file << endl;
                    }

                    cout << "DECK: ";
                    printCardsToStream(&deck, &cout);
                    cout << endl;

                    turn++;

                    pthread_mutex_unlock(&mutex);
                    sleep(1);
                }
            }
        }

        //End of round
        pthread_mutex_lock(&deckFullMutex);

        //Dealer doesn't have to say they lost
        if(id != roundNum) {
            file << "PLAYER " << id + 1 << ": ";
            if(winner == id) {
                file << "wins round " << roundNum + 1 << endl;
            }
            else {
                file << "loses round " << roundNum + 1 << endl;
             }
        }

        //If you have cards, discard all of them
        if(hands[id].size() > 0)
            discard(id, true);

        //If all cards have been returned to the deck, end the round
        if(deck.size() == 52) {
            roundStart = false;
            roundNum++;
            pthread_cond_broadcast(&deckFullCond);
        }

        //Otherwise, wait for the round to end
        else {
            while(deck.size() < 52) {
                pthread_cond_wait(&deckFullCond, &deckFullMutex);
            }
        }

        if(id == (roundNum - 1)) {
                file << "PLAYER " << id + 1 << ": Round ends" << endl << endl;
                cout << "PLAYER " << id + 1 << ": Round ends" << endl << endl;
        }
        pthread_mutex_unlock(&deckFullMutex);
        sleep(1);

    }

    pthread_exit(NULL);
}