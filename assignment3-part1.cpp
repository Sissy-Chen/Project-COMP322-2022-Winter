#include <iostream>
#include <vector>
#include <algorithm>
#include <random>
#include <chrono>
#include <exception>
using namespace std;

/***
 * Global variables
 ***/
int aceValue;
int roundsWonByPlayer = 0;
int roundsWonByCasino = 0;
int totalRoundsWon = 0;  // does not account for rounds ending in a push
float winRatioFloor = 0.55;
float winRatioCeiling = 0.65;
/***
 * initialized to 0.6 when no round has been played to not trigger rigging logic in the first round
 * updated after each round played
 ***/
float actualWinRatio = 0.60;

/***
 * Classes
 ***/
class Card
{
    public:
        enum Rank { ACE = 1, TWO, THREE, FOUR, FIVE, SIX, SEVEN, EIGHT, NINE, TEN, JACK, QUEEN, KING };
        enum Type { CLUBS, DIAMONDS, HEARTS, SPADES };
        Card(Rank r, Type t) // non-default constructor
        {
            this->rank = r;
            this->type = t;
        }
        /***
         * helper method to display rank of card
         * called in Card::displayCard()
         ***/
        string to_string(Rank rankEnum)
        {
            string rankString;
            switch(rankEnum)
            {
                case JACK:
                    rankString = "J";
                    break;
                case QUEEN:
                    rankString = "Q";
                    break;
                case KING:
                    rankString = "K";
                    break;
                default:
                    rankString = std::to_string(rankEnum);
                    break;
            }
            return rankString;
        }
        /***
         * helper method to display type of card
         * called in Card::displayCard()
         ***/
        char to_char(Type typeEnum)
        {
            char typeChar;
            switch(typeEnum)
            {
                case CLUBS:
                    typeChar = 'C';
                    break;
                case DIAMONDS:
                    typeChar = 'D';
                    break;
                case HEARTS:
                    typeChar = 'H';
                    break;
                case SPADES:
                    typeChar = 'S';
                    break;
            }
            return typeChar;
        }
        int getValue()
        {
            int val;
            switch(rank)
            {
                case Rank::ACE:
                    val = aceValue;  // 1 or 11, as decided by player/casino each time they are dealt an Ace
                    break;
                case Rank::JACK:
                case Rank::QUEEN:
                case Rank::KING:
                    val = 10;
                    break;
                default:
                    val = tolower(rank);
                    break;
            }
            return val;
        }
        void displayCard()
        {
            cout << to_string(rank) << to_char(type);
        }
        /***
         * helper method for the player to choose the value of the Ace they are dealt
         * called in Hand::deal()
         ***/
        static int setAceValue() {
            string input;
            while (input != "1" && input != "11")
            {
                cout << "You drew an Ace! How many points would you like it to be worth? (1/11):";
                cin >> input;
                if (input == "1" || input == "11")
                {
                    aceValue = stoi(input);
                    if (aceValue == 1 || aceValue == 11)
                    {
                        return 0;
                    }
                }
                else {
                    cout << "Incorrect value entered. Face value of Ace can be either 1 or 11. Please try again." << endl;
                }
            }
            return -1;
        }
        /***
         * helper method for the casino to choose the value of the Ace they are dealt
         * called in Hand::deal()
         ***/
        static int setAceValue(int i)
        {
            if (i == 1 || i == 11)
            {
                aceValue = i;
                return 0;
            }
            else
            {
                return -1;
            }
        }
        /***
         * helper method for retrieving the rank of a card
         * called in Hand::deal()
         ***/
        Card::Rank getRank()
        {
            return this->rank;
        }
    private:
        Rank rank;
        Type type;
};

class Hand
{
    public:
        virtual ~Hand() {}  // destructor marked as virtual since class is meant to be inherited
        int add(Card c)
        {
            int size = cards.size();
            cards.push_back(c);
            total += c.getValue();  // update total incrementally, specifically because the value of Ace can vary (1/11) for each Ace drawn
            if (cards.size() == size+1)
            {
                return 0;
            }
            else
            {
                return -1;
            }
        }
        int clear()
        {
            cards.clear();
            total = 0;  // reset total
            if (cards.size() == 0)
            {
                return 0;
            }
            else
            {
                return -1;
            }
        }
        int getTotal() const  // marked const to be called in isDrawing()
        {
            return total;
        }
        /***
         * helper method for printing hand of players
         * called in BlackJackGame::play()
         ***/
        void displayCards()
        {
            for (Card c : cards)
            {
                c.displayCard();
                cout << " ";
            }
            cout << "[" << getTotal() << "]" << endl;
        }
    protected:
        vector<Card> cards;
        int total = 0;
};

class Deck: public Hand
{
    public:
        int populate()
        {
            for (int rankInt = Card::Rank::ACE; rankInt <= Card::Rank::KING; rankInt++)
            {
                for (int typeInt = Card::Type::CLUBS; typeInt <= Card::Type::SPADES; typeInt++)
                {
                    Card::Rank r = static_cast<Card::Rank>(rankInt);
                    Card::Type t = static_cast<Card::Type>(typeInt);
                    Card newCard = Card(r, t);
                    cards.push_back(newCard);
                }
            }
            if (cards.size() == 52)
            {
                return 0;
            }
            else
            {
                return -1;
            }
        }
        int shuffle()
        {
            int deckSize = cards.size();
            unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
            std::shuffle(begin(cards), end(cards), std::default_random_engine(seed));
            if (cards.size() == deckSize)
            {
                return 0;
            }
            else
            {
                return -1;
            }
        }
        /***
         * declaration of friend function
         ***/
        template <typename T>
        friend int deal(Deck& d, T& hand);
        /***
         * helper method for rigging logic
         * called in ComputerPlayer::isDrawing()
         * 
         * triggered in favour of casino when casino's percentage of winning < 55%
         * triggered in favour of player when casino's percentage of winning > 65%
         ***/
        static int rigDeck(Deck& d, int val)
        {
            int numCardsMoved = 0;
            int size = d.cards.size();
            int pointsToAchieve = val;
            int max;
            while (val > 0)
            {
                if (val >= 11 && aceValue == 11)
                {
                    max = 11;
                }
                else  // 10 or less
                {
                    max = val;
                }
                int toMoveToTheBack;  // index of card to move to the back of the deck
                for (int n = 0; n < size; n++)  // starting from front of the deck
                {
                    if (d.cards.at(n).getValue() == max)
                    {
                        auto it = d.cards.begin() + n;
                        rotate(it, it + 1, d.cards.end());  // move card at index n to the back of the deck
                        numCardsMoved++;
                        break;
                    }
                }
                val -= max;  // update remaining points to achieve
            }
            int pointsToScore = 0;
            for (int i = size-1; i > size-1-numCardsMoved; i--)  // starting from the back of the deck
            {
                pointsToScore += d.cards.at(i).getValue();
            }
            if (pointsToScore == pointsToAchieve)
            {
                //cout << "Rigged, sorry ;)" << endl;
                return 0;
            }
            else
            {
                return -1;
            }
        }
};

/***
 * abstract base class
 * 1 pure virtual method: isDrawing()
 * 
 * inherited by ComputerPlayer, HumanPlayer
 ***/
class AbstractPlayer: public Hand
{
    public:
        virtual ~AbstractPlayer() {}  // destructor marked as virtual since class is meant to be inherited
        /***
         * no implementation
         * must be implemented in derived classes: ComputerPlayer, HumanPlayer
         ***/
        virtual bool isDrawing() const = 0;
        /***
         * "busted" defined identically for player and casino (score over 21)
         ***/
        bool isBusted()
        {
            return (getTotal() > 21);
        }
};

/***
 * ComputerPlayer is-a AbstractPlayer
 ***/
class ComputerPlayer: public AbstractPlayer
{
    public:
        ComputerPlayer(Deck& deck):d(deck) {}  // non-default constructor
        virtual bool isDrawing() const  // marked virtual for readability
        {
            if (getTotal() <= 16)
            {
                // takes the casino's percentage of winning into consideration
                if (actualWinRatio < winRatioFloor)  // casino's current percentage of winning < 55%
                {
                    int pointsToAchieve = 21 - getTotal();
                    /***
                     * rig the game by rearranging the deck such that the next card(s) dealt to the casino
                     * achieve a score of pointsToAchieve to allow the casino to overall score exactly 21
                    ***/
                    Deck::rigDeck(d, pointsToAchieve);
                }
            }
            return (getTotal() <= 16);
        }
    private:
        Deck& d;
};

/***
 * HumanPlayer is-a AbstractPlayer
 ***/
class HumanPlayer: public AbstractPlayer
{
    public:
        HumanPlayer(Deck& deck):d(deck) {}  // non-default constructor
        virtual bool isDrawing() const  // marked virtual for readability
        {
            if (getTotal() < 21)
            {
                /***
                 * ensure player win when casino's percentage of winning goes > 65%
                 * so that the casino doesn't win exceedingly often
                 ***/
                if (actualWinRatio > winRatioCeiling)  // casino's current percentage of winning > 65%
                {
                    int pointsToAchieve = 21 - getTotal();
                    /***
                     * rig the game by rearranging the deck such that the next card(s) dealt to the player
                     * achieve a score of pointsToAchieve to allow the player to overall score exactly 21
                    ***/
                    Deck::rigDeck(d, pointsToAchieve);
                }
            }
            return (getTotal() < 21);
        }
        void announce(ComputerPlayer& casino)
        {
            int playerScore = getTotal();  // final player score
            int casinoScore = casino.getTotal();  // final casino score
            if ((isBusted() && casino.isBusted()) || isBusted() || (casinoScore == 21 && playerScore < 21) || (casinoScore < 21 && playerScore < 21 && casinoScore > playerScore))
            {
                cout << "Casino wins." << endl;
                // update number of casino wins
                roundsWonByCasino++;
                // update number of rounds won by either player or casino
                totalRoundsWon++;
            }
            else if (casino.isBusted() || (playerScore == 21 && casinoScore < 21) || (playerScore < 21 && casinoScore < 21 && playerScore > casinoScore))
            {
                cout << "Player wins." << endl;
                // update number of player wins
                roundsWonByPlayer++;
                // update number of rounds won by either player or casino
                totalRoundsWon++;
            }
            else if (playerScore == casinoScore)
            {
                cout << "Push: No one wins." << endl;
            }
            // update casino's current percentage of winning
            if (totalRoundsWon > 0)
            {
                actualWinRatio = (float)roundsWonByCasino/(float)totalRoundsWon;
            }
            else
            {
                actualWinRatio = 0.0;
            }
        }
    private:
        Deck& d;
};

/***
 * implementation of friend function
 ***/
template <typename T>  // function template
int deal(Deck& d, T& hand)
{
    if (!d.cards.empty())  // deck is non-empty
    {
        int deckSize = d.cards.size();
        Card dealt_card = d.cards.back();
        if (dealt_card.getRank() == Card::Rank::ACE)  // if dealt card is an Ace
        {
            if (std::is_same<T, ComputerPlayer>::value)  // hand belongs to the casino
            {
                // choose value of the drawn Ace to maximize score without busting
                int currentScore = hand.getTotal();
                if (21-currentScore == 11 || currentScore+11 <= 16)
                {
                    Card::setAceValue(11);
                }
                else
                {
                    Card::setAceValue(1);
                }
            }
            else  // hand belongs to player
            {
                Card::setAceValue();  // let player choose value of the drawn Ace (1/11)
            }
        }
        hand.add(dealt_card);  // add card to hand
        d.cards.pop_back();  // remove card from deck
        if (d.cards.size() == deckSize-1)
        {
            return 0;
        }
        else
        {
            return -1;
        }
    }
    else  // no card left to deal
    {
        cout << "No more cards left in the deck" << endl;
        return 0;
    }
}

class BlackJackGame
{
    public:
        int play()
        {
            try {
                char answer;
                // each round starts with a full deck
                m_deck.populate();
                // shuffle deck before dealing cards
                m_deck.shuffle();

                // deal 1 card to Casino
                deal<ComputerPlayer>(m_deck, m_casino);
                // display Casino's first card
                cout << "Casino: ";
                m_casino.displayCards();
                // deal 2 cards to Player
                deal<HumanPlayer>(m_deck, m_player);
                deal<HumanPlayer>(m_deck, m_player);
                // display Player's first 2 cards
                cout << "Player: ";
                m_player.displayCards();

                while (m_player.isDrawing())  // Player's total < 21, their turn to draw
                {
                    cout << "Do you want to draw? (y/n): ";
                    cin >> answer;
                    if (answer == 'y')  // Player decides to draw (again)
                    {
                        deal<HumanPlayer>(m_deck, m_player);
                        cout << "Player: ";
                        m_player.displayCards();
                        if (m_player.isBusted() || m_player.getTotal() == 21)  // Player score >= 21, can no longer draw
                        {
                            break;
                        }
                    }
                    else  // Player decides not to draw
                    {
                        break;
                    }
                }
                if (m_player.isBusted())
                {
                    m_player.announce(m_casino);
                }
                else  // Player score <= 21
                {
                    m_casino.isDrawing();  // rearrange deck in favour of Casino if Casino's percentage of winning < 55%
                    deal<ComputerPlayer>(m_deck, m_casino);
                    cout << "Casino: ";
                    m_casino.displayCards();
                    while (m_casino.isDrawing())  // Casino's total <= 16, Casino's turn to draw
                    {
                        deal<ComputerPlayer>(m_deck, m_casino);
                        cout << "Casino: ";
                        m_casino.displayCards();
                        if (m_casino.isBusted() || m_casino.getTotal() == 21)
                        {
                            break;
                        }
                    }
                    m_player.announce(m_casino);
                }

                // clear player hands and deck
                m_casino.clear();
                m_player.clear();
                m_deck.clear();
                //cout << "Casino winning rate: " << actualWinRatio*100 << "%" << endl;
            }
            catch (std::exception& e)
            {
                cout << e.what() << endl;
            }
            if (m_casino.getTotal() == 0 && m_player.getTotal() == 0 && m_deck.getTotal() == 0)
            {
                return 0;
            }
            else
            {
                return -1;
            }
        }
    private:
        Deck m_deck;
        ComputerPlayer m_casino = ComputerPlayer(m_deck);
        HumanPlayer m_player = HumanPlayer(m_deck);
};

/***
 * Main
 ***/
int main()
{
    cout << "\tWelcome to the Comp322 Blackjack game!" << endl << endl;
    
    BlackJackGame game;
    
    // The main loop of the game
    bool playAgain = true;
    char answer = 'y';
    while (playAgain)
    {
        game.play();
        
        // Check whether the player would like to play another round
        cout << "Would you like another round? (y/n): ";
        cin >> answer;
        cout << endl << endl;
        playAgain = (answer == 'y' ? true : false);
    }
    
    cout <<"Game over!";
    return 0;
}