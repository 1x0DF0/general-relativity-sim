// "camera" used for our player/agent/obsever
//To visualize black holes, wormholes, or quantum spacetime fluctuations
// we must write a custom relativistic raymarcher.

#include <array>

class relativistic_raymarcher {

  private:


  public:



};

struct photon {

  std::array<double, 4> position;

  std::array<double, 4> velicoty; // Null vector (ds^2 = 0)

  std::array<double, 4> acceleration;

  std::array<double, 4> density;

};