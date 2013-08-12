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

#include "Glicko2.h"

#include <cmath>


#define PI 3.14159265358979323846f
#define PI_SQUARED 9.86960440108935861883f
#define tau 0.5f // Internal glicko2 parameter. "Reasonable choices are between 0.3 and 1.2, though the system should be tested to decide which value results in greatest predictive accuracy."
#define epsilon 0.000001f //convergence tolerance

Player::Player()
{
  setRating(Initial_Rating);
  setRd(Initial_Diviation);
  volatility = Initial_Volatility;
}

Player::Player(float R, float RD, float Vol)
{
  setRating(R);
  setRd(RD);
  volatility = Vol;
}

Player::~Player()
{
}


// The Glicko-2 system is specified on http://www.glicko.net/
Glicko2::Glicko2()
{
}

Glicko2::~Glicko2()
{
}

// Returns 1 / sqrt(1 + (3 * deviation ^ 2) / pi ^ 2)
float Glicko2::g(const float deviation)
{
  return 1.0f / ( sqrt(1.0f + 3.0f * pow(deviation, 2) / PI_SQUARED) );
}

// Returns 1 / (1 + exp(-g(deviation_opponent)) * (rating - opponentRating)))
float Glicko2::E(const float rating, const float rating_opponent, const float deviation_opponent)
{
  return 1.0f / (1.0f + exp( -g(deviation_opponent) * (rating - rating_opponent) ) );
}

// Step 3
// Calculation of the estimated variance of the player's rating based on game outcomes
float Glicko2::CalculateVariance(const Player &player, const std::vector<Match> &matches)
{
  float variance = 0;
  const Player * opponent;
  
  for (auto it = matches.begin(); it != matches.end(); ++it)
  {
    opponent = it->opponent;
    float E_i = E(player.rating, opponent->rating, opponent->deviation);
    variance += pow( g(opponent->deviation), 2) * E_i * (1.0f - E_i);
  }

  return 1.0f / variance;
}

// The delta function of the Glicko2 system.
// Calculation of the estimated improvement in rating (step 4 of the algorithm)
float Glicko2::CalculateDelta(const Player &player,const std::vector<Match> &matches, const float v)
{
  float part_delta_sum = 0;
  const Player * opponent;

  for (auto it = matches.begin(); it != matches.end(); ++it)
  {
    opponent = it->opponent;
    part_delta_sum += opponent->deviation * (it->result - E(player.rating, opponent->rating, opponent->deviation));
  }

  return part_delta_sum * v;
}

// Calculating the new volatility as per the Glicko2 system.
float Glicko2::newVolatility(Player &player, const std::vector<Match> &matches, const float v)
{
  // Step 4  
  const float delta = CalculateDelta(player, matches, v);

  // Step 5
  // 5.1
  const float a = log(pow(player.volatility, 2));
  auto f = [=](float x)
  { 
    return exp(x) * (pow(delta, 2) -pow(player.deviation, 2) - v - exp(x)) / (2 * pow(pow(player.deviation, 2) + v + exp(x), 2)) - (x - a) / pow(tau, 2);
  };
  
  // 5.2
  float A = a;
  float B, k;
  if (pow(delta, 2) > pow(player.deviation, 2) + v)
  {
    B = log(pow(delta, 2) - pow(player.deviation, 2) - v);
  }
  else
  {
    k = 1.0f;
    while (f(a - k * tau) < 0)
    {
      k = k + 1;
    }
    B = a - k * tau;
  }

  // 5.3
  float fA = f(A);
  float fB = f(B);
  float C, fC;

  // 5.4
  while (abs(B - A) > epsilon)
  {
    C = A + (A - B) * fA / (fB - fA);
    fC = f(C);
    if (fC * fB < 0)
    {
      A = B;
      fA = fB;
    }
    else 
    {
      fA = fA / 2;
    }
    B = C;
    fB = fC;
  }

  // 5.5
  return exp(A/2);
}

// Step 6
inline float Glicko2::preRatingRD(const float deviation, const float v)
{
  return sqrt( pow(deviation, 2) + pow(v, 2) );
}

void Glicko2::calculateNewRatings(Player &player, const std::vector<Match> &matches, const float factor)
{
  const Player old = player;
  // Step 1: done by Player initialization
  // Step 2: done by setRating and setRd

  // Step 3
  const float v = CalculateVariance(player, matches);

  // Step 4 & 5
  player.volatility = newVolatility(player, matches, v);

  // Step 6
  player.deviation = preRatingRD(player.deviation, player.volatility);

  // Step 7
  player.deviation = 1 / sqrt((1 / pow(player.deviation, 2)) + (1 / v));

  float rd_sum = 0;
  const Player * opponent;
  
  for (auto it = matches.begin(); it != matches.end(); ++it)
  {
    opponent = it->opponent;
    rd_sum += g(opponent->deviation) * (it->result - E(player.rating, opponent->rating, opponent->deviation));
  }

  player.rating = pow(player.deviation, 2) * rd_sum;

  // Step 8: Done by getRating and GetRd

  //adjust the values based on the factor used
  player.volatility = (player.volatility - old.volatility) * factor + old.volatility;
  player.deviation = (player.deviation - old.deviation) * factor + old.deviation;
  player.rating = (player.rating - old.rating) * factor + old.rating;
}

// Used to adjust the scores in case of a multi sided battle
float Glicko2::adjustScore(const float ratingA, const float ratingB)
{
  float percent = ratingA / (ratingA + ratingB);
  return (sin( (percent - 0.5f) * PI) + 1.0f) * 0.5f;
}

//Most matches are done as part of a team. So we need to take the average of each team member to get an accurate representation of the strength of the team
void Glicko2::calculateTeamRatings(const std::vector<PlayerMatch> &teams)
{
  //process all the teams
  for (auto teamIt = teams.begin(); teamIt != teams.end(); ++teamIt)
  {
    size_t length = teamIt->matches->size();
    Player * player = teamIt->player;
    const std::vector<Match> * matches = teamIt->matches;

    //we need to process every match of the team
    for (auto it = matches->begin(); it != matches->end(); ++it)
    {
      calculateNewRatings(*player, *matches, 1.0f / length);
    }
  }
}
