#include <iostream>
#include <cstdlib>
#include <cstring>
#include <string>
#include <fstream>
#include <sstream>
#include <regex>
#include <assert.h>
#include <exception>

#include <optixu/optixpp_namespace.h>
#include <optixu/optixu_math_namespace.h>

using namespace std;
using namespace optix;


class init_exception : public std::runtime_error
{
public:
    init_exception(const std::string& error): std::runtime_error(error){}
};

//-----------------------------------------------------------------------------
// 
// Helpers 
//
//-----------------------------------------------------------------------------

static float randf() 
{ 
  return static_cast<float>( rand() ) / static_cast<float>( RAND_MAX ); 
}


static void setBufferIds( const vector<optix::Buffer>& buffers,
                   optix::Buffer top_level_buffer )
{
  top_level_buffer->setSize( buffers.size() );
  int* data = reinterpret_cast<int*>( top_level_buffer->map() );
  for( unsigned i = 0; i < buffers.size(); ++i )
    data[i] = buffers[i]->getId();
  top_level_buffer->unmap();
}

static vector<string> splitString(const string & source, const string & delimiter) {
  vector<string> result;
  if (delimiter.empty()) {
    result.push_back(source);
    return result;
  }
  string::const_iterator substart = source.begin(), subend;
  while (true) {
    subend = search(substart, source.end(), delimiter.begin(), delimiter.end());
    string sub(substart, subend);
    if (!sub.empty()) {
      result.push_back(sub);
    }
    if (subend == source.end()) {
      break;
    }
    substart = subend + delimiter.size();
  }
  return result;
}

template<typename T>
static inline T strTo(const string& intString) {
  T x;
  istringstream cur(intString);
  cur >> x;
  return x;
}

static inline float3 interpolate( const float3& begin, const float3& end, float p) {
  float3 tmp;
  tmp.x = begin.x*(1-p) + end.x*p;
  tmp.y = begin.y*(1-p) + end.y*p;
  tmp.z = begin.z*(1-p) + end.z*p;
  return tmp;
}

static inline float3 strToColor(const string& hexString) {
  // unsigned int x = std::stoul(s, nullptr, 16);
  float3 color;
  unsigned int hex;
  stringstream ss;
  ss << std::hex << hexString;
  ss >> hex;

  color.x = ((hex >> 16) & 0xFF ) / 255.f;
  color.y = ((hex >> 8) & 0xFF ) / 255.f;
  color.z = ( hex & 0xFF ) / 255.f;
  return color;
}

