/*

  Copyright (c) 2013 Tim Donks
  
  This software is provided 'as-is', without any express or implied warranty. In
  no event will the authors be held liable for any damages arising from the use
  of this software.
  
  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it freely,
  subject to the following restrictions:
  
  1. The origin of this software must not be misrepresented; you must not claim
     that you wrote the original software. If you use this software in a
     product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  
  3. This notice may not be removed or altered from any source distribution.

*/

#ifndef __glicko2_h__
#define __glicko2_h__

#include <vector>


#define Glicko2_Factor 173.7178f //value required to calculates the values to the glicko2 scale
#define Initial_Rating 1500.0f
#define Initial_Diviation 350.0f
#define Initial_Volatility 0.06f

class Player
{
public:
  Player();
  Player(float R, float RD, float Vol = Initial_Volatility);
  ~Player();

  //internally Glicko2 uses a different rating from Glicko. These get and set functions will do the conversions that are needed
  inline void setRating(const float R)
  {
    rating = (R - 1500.0f) / Glicko2_Factor;
  }

  inline void setRd(const float Rd)
  {
    deviation = Rd / Glicko2_Factor;
  }

  inline float getRating()
  {
    return Glicko2_Factor * rating + 1500.0f;
  }

  inline float getRd()
  {
    return Glicko2_Factor * deviation;
  }

  // rating data
  float rating, deviation, volatility;
};

struct Match
{
  Match(Player * opponent, float result) : opponent(opponent), result(result) {}
  
  Player * opponent;
  float result; //outcome 1 for player victory, 0 for opponent victory, 0.5 for draws
};

struct PlayerMatch
{
  PlayerMatch(Player * player, std::vector<Match> * matches) : player(player), matches(matches) {}
  
  Player * player;
  std::vector<Match> * matches;
};

// The Glicko-2 system is specified on http://www.glicko.net/
class Glicko2
{
public:
  Glicko2();
  ~Glicko2();
        
  void calculateNewRatings(Player &player, const std::vector<Match> &matches, const float factor = 1.0f);
  float adjustScore(const float ratingA, const float ratingB);
  void calculateTeamRatings(const std::vector<PlayerMatch> &teams);

private:
  // utility functions
  float g(const float deviation);
  float E(const float rating, const float rating_opponent, const float deviation_opponent);
  float CalculateVariance(const Player &player, const std::vector<Match> &matches);
  float CalculateDelta(const Player &player,const std::vector<Match> &matches, const float v);
  float newVolatility(Player &player, const std::vector<Match> &matches, const float v);
  float preRatingRD(const float deviation, const float v);
};

#endif // __glicko2_h__