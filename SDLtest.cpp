
#include <stdio.h>
#include <time.h>

#include <dinrhiw.h>

#include <string>
#include <chrono>


#define USESDL

#ifdef USESDL
extern "C" {
#include <SDL.h>
#include <SDL_ttf.h>
#include <SDL_image.h>
#include <SDL_mixer.h>
}
#endif


#include "hermitecurve.h"
#include "SDLAVCodec.h"



Uint32 getpixel(SDL_Surface *surface, int x, int y)
{
  int bpp = surface->format->BytesPerPixel;
  /* Here p is the address to the pixel we want to retrieve */
  Uint8 *p = (Uint8 *)surface->pixels + y * surface->pitch + x * bpp;
  
  return *(Uint32 *)p;
}


void setpixel(SDL_Surface *surface, int x, int y, Uint32 data)
{
  int bpp = surface->format->BytesPerPixel;
  /* Here p is the address to the pixel we want to retrieve */
  Uint8 *p = (Uint8 *)surface->pixels + y * surface->pitch + x * bpp;
  
  *(Uint32 *)p = data;
}




void getRgbPixels(SDL_Surface* s, int x, int y, Uint8& r, Uint8&g, Uint8& b){
  Uint32 data = getpixel(s, x, y);
  SDL_GetRGB(data, s->format, &r, &g, &b);
}


void setRgbPixels(SDL_Surface* s, int x, int y, Uint8 r, Uint8 g, Uint8 b){
  Uint32 data = ((Uint32)r) + (((Uint32)g)<<8) + (((Uint32)b)<<16) + 0x00000000; // 0%/50%/100% alpha mode.. 
  setpixel(s, x, y, data);
}

  
void floodfill(const int x, const int y,
	       SDL_Surface* s, const Uint8 r, const Uint8 g, const Uint8 b)
{
  Uint8 rg,gg,bg;

  std::vector< std::pair<int, int> > coords; 
  
  coords.push_back(std::pair<int,int>(x,y));

  while(coords.size() > 0){
    auto iter = coords.end();
    iter--;

    const int x = iter->first;
    const int y = iter->second;

    coords.erase(iter);

    if (x<0 || y<0 || x>=s->w || y>=s->h ){
      continue;
    }

    getRgbPixels(s,x,y,rg,gg,bg);
    
    if (rg==0xFF && gg== 0xFF && bg== 0xFF) continue;
    if (rg==r && gg== g && bg== b) continue;
    
    setRgbPixels(s,x,y,r,g,b);

    coords.push_back(std::pair<int,int>(x+1,y));
    coords.push_back(std::pair<int,int>(x-1,y));
    coords.push_back(std::pair<int,int>(x,y+1));
    coords.push_back(std::pair<int,int>(x,y-1));
  }
  
}





using namespace whiteice;
using namespace whiteice::resonanz;



bool renderPlot(const unsigned long long tick,
		const double phase1, const double phase2, const double phase3,
		double& curveParameter,
		unsigned long long& latestTickCurveDrawn,
		const double TICKSPERCURVE,
		std::vector< whiteice::math::vertex< whiteice::math::blas_real<double> > >& startPoint,
		std::vector< whiteice::math::vertex< whiteice::math::blas_real<double> > >& endPoint,
		SDL_Surface* surface);



int main(int argc, char** argv)
{
  srand(time(0));
  
  printf("Charm [64KB intro] by Sensar Studios\n");
  fflush(stdout);

  const std::string windowTitle = "Charm [64KB]";
  const std::string audiofile = "charm.mp3";
  const std::string fontname = "Vera.ttf";
  

#ifdef USESDL
  
  SDL_Window* window = NULL;

  SDL_Init(0);
  
  SDL_DisplayMode mode;
  int SCREEN_WIDTH = 800;
  int SCREEN_HEIGHT = 600;

  if(SDL_InitSubSystem(SDL_INIT_EVENTS) != 0){
    return -1;;
  }
  
  if(SDL_InitSubSystem(SDL_INIT_VIDEO) != 0){
    return -1;
  }

  if(SDL_InitSubSystem(SDL_INIT_AUDIO) != 0){
    return -1;
  }
  
  if(TTF_Init() != 0){
    return -1;
  }

  if(Mix_Init(MIX_INIT_MP3) != MIX_INIT_MP3){
    return -1;
  }
  
  if(Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 4096) == -1){
    return -1;
  }


  

  
  if(SDL_GetCurrentDisplayMode(0, &mode) == 0){
    SCREEN_WIDTH = mode.w;
    SCREEN_HEIGHT = mode.h;
  }

#if 1
  window = SDL_CreateWindow(windowTitle.c_str(),
			    SDL_WINDOWPOS_CENTERED,
			    SDL_WINDOWPOS_CENTERED,
			    (3*SCREEN_WIDTH)/4, (3*SCREEN_HEIGHT)/4,
			    SDL_WINDOW_SHOWN);
#endif

#if 0
  window = SDL_CreateWindow(windowTitle.c_str(),
			    SDL_WINDOWPOS_CENTERED,
			    SDL_WINDOWPOS_CENTERED,
			    SCREEN_WIDTH, SCREEN_HEIGHT,
			    SDL_WINDOW_SHOWN | SDL_WINDOW_FULLSCREEN_DESKTOP);
#endif
  
  if(window == NULL) return -1;

  SDL_GetWindowSize(window, &SCREEN_WIDTH, &SCREEN_HEIGHT);

  double fontSize = 50.0*sqrt(((float)(SCREEN_WIDTH*SCREEN_HEIGHT))/(640.0*480.0));
  unsigned int fs = (unsigned int)fontSize;
  if(fs <= 0) fs = 10;

  TTF_Font* font = nullptr;
  font = 0;
  font = TTF_OpenFont(fontname.c_str(), fs);

  Mix_Music* music = Mix_LoadMUS(audiofile.c_str());
  if(music){
    if(Mix_PlayMusic(music, -1) == -1){
      return -1;
    }
  }

  
  
  SDL_SetWindowGrab(window, SDL_TRUE);
  SDL_UpdateWindowSurface(window);
  SDL_RaiseWindow(window);

  bool running = true;

  SDL_Event event;

  const unsigned int NUMBLOBS = 3; // number of graphic elements in effect..

  double phase1[NUMBLOBS];
  double phase2[NUMBLOBS];
  double phase3[NUMBLOBS];

  double curveParameter[NUMBLOBS];
  const double TICKSPERCURVE = 10;
  unsigned long long latestTickCurveDrawn[NUMBLOBS];
  
  std::vector< whiteice::math::vertex< whiteice::math::blas_real<double> > > startPoint[NUMBLOBS];
  std::vector< whiteice::math::vertex< whiteice::math::blas_real<double> > > endPoint[NUMBLOBS];

  for(unsigned int i=0;i<NUMBLOBS;i++){
    phase1[i] = rng.uniform().c[0];
    phase2[i] = rng.uniform().c[0];
    phase3[i] = rng.uniform().c[0];

    curveParameter[i] = 10.0;
    latestTickCurveDrawn[i] = 0;
  }
  
  
  
  unsigned long long tick = 0;

  {
    SDL_Surface* surface = SDL_GetWindowSurface(window);
    SDL_FillRect(surface, NULL, 0x80FFFFFF);
    SDL_FreeSurface(surface);
  }

  SDL_Surface* black = SDL_CreateRGBSurface(0, SCREEN_WIDTH, SCREEN_HEIGHT, 32,
					    0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000);
  SDL_FillRect(black, NULL, 0xA0FFFFFF);
  SDL_SetSurfaceBlendMode(black, SDL_BLENDMODE_BLEND);
  
  SDLAVCodec* video = new SDLAVCodec(0.50f);
  if(video->startEncoding("intro.mp4", SCREEN_WIDTH, SCREEN_HEIGHT) == false)
    return -1;


  auto t0 = std::chrono::system_clock::now().time_since_epoch();
  auto t0ms = std::chrono::duration_cast<std::chrono::milliseconds>(t0).count();
  unsigned long long programStarted = t0ms;
  

  while(running){
    tick++;

    std::vector<std::string> message;
    message.push_back("Charm");
    message.push_back("[sensar studios]");

    SDL_Surface* surface = SDL_GetWindowSurface(window);

    SDL_BlitSurface(black, NULL, surface, NULL);

    SDL_Surface* pic[10];

#pragma omp parallel for
    for(unsigned int i=0;i<NUMBLOBS;i++){
      pic[i] = SDL_CreateRGBSurface(0, SCREEN_WIDTH, SCREEN_HEIGHT, 32,
				    0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000);
      
      renderPlot(tick, phase1[i], phase2[i], phase3[i],
		 curveParameter[i],
		 latestTickCurveDrawn[i],
		 TICKSPERCURVE,
		 startPoint[i],
		 endPoint[i],
		 pic[i]);
      
      SDL_SetSurfaceBlendMode(pic[i], SDL_BLENDMODE_BLEND);
    }

    for(unsigned int i=0;i<NUMBLOBS;i++){
      SDL_BlitSurface(pic[i], NULL, surface, NULL);
      SDL_FreeSurface(pic[i]);

    }
      
    
    {
      const SDL_Color white = { 255, 255, 255 };
      
      for(unsigned int i=0;i<message.size();i++){
	
	SDL_Surface* msg = TTF_RenderUTF8_Blended(font, message[i].c_str(), white);
	
	SDL_Rect messageRect;
	
	messageRect.x = (SCREEN_WIDTH - msg->w)/2;
	messageRect.y = (SCREEN_HEIGHT - 2*(msg->h)*(message.size()-i))/2;
	messageRect.w = msg->w;
	messageRect.h = msg->h;
	
	if(SDL_BlitSurface(msg, NULL, surface, &messageRect) != 0)
	  return false;
	
	SDL_FreeSurface(msg);
      }
    }

    // update video recorder
    {
      auto t1 = std::chrono::system_clock::now().time_since_epoch();
      auto t1ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1).count();
      
      if(video->insertFrame((unsigned long long)(t1ms - programStarted),
			    surface) == false){
	printf("video->insertFrame() FAILED.\n");
	return -1; 
      }
    }

    SDL_FreeSurface(surface);
    
    SDL_UpdateWindowSurface(window);
    
    while(SDL_PollEvent(&event)){ 
      if(event.type == SDL_KEYDOWN &&
	 (event.key.keysym.sym == SDLK_ESCAPE ||
	  event.key.keysym.sym == SDLK_RETURN)
	 )
	{
	  running = false;
	}
    }

  }

  {
    auto t1 = std::chrono::system_clock::now().time_since_epoch();
    auto t1ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1).count();
    
    video->stopEncoding((unsigned long long)(t1ms - programStarted));
  }

  SDL_Quit();
  
#endif
    
  return 0;
}





bool renderPlot(const unsigned long long tick,
		const double phase1, const double phase2, const double phase3,
		double& curveParameter,
		unsigned long long& latestTickCurveDrawn,
		const double TICKSPERCURVE,
		std::vector< whiteice::math::vertex< whiteice::math::blas_real<double> > >& startPoint,
		std::vector< whiteice::math::vertex< whiteice::math::blas_real<double> > >& endPoint,
		SDL_Surface* surface)
{
  const unsigned int SCREEN_WIDTH = surface->w;
  const unsigned int SCREEN_HEIGHT= surface->h;

  const double t = tick/25.0;
  
  const double w1 = 1.0;
  const double w2 = 0.3333333;
  const double w3 = 3.1415927;
  
  const double angle1 = w1*t + phase1;
  const double angle2 = w2*t + phase2;
  const double angle3 = w3*t + phase3;
  
  
  {
    {
      unsigned int r = 0xFF & rand();
      unsigned int g = 0xFF & rand();
      unsigned int b = 0xFF & rand();
      
      r = 0xFF*((1.0 + sin(angle1))/2.0);
      g = 0xFF*((1.0 + cos(angle2))/2.0);
      b = 0xFF*((1.0 + sin(cos(angle3)))/2.0);
      
      if(r > 0xFF) r = 0xFF;
      if(g > 0xFF) g = 0xFF;
      if(b > 0xFF) b = 0xFF;
      
      SDL_FillRect(surface, NULL, SDL_MapRGBA(surface->format, r, g, b, 0x80));
    }
    
    {
      std::vector< math::vertex< math::blas_real<double> > > curve;
      std::vector< whiteice::math::vertex< whiteice::math::blas_real<double> > > points;
      const unsigned int NPOINTS = 5;
      const unsigned int DIMENSION = 3;
      
      {
	points.resize(NPOINTS);
	
	if(curveParameter > 1.0)
	{
	  points.resize(NPOINTS);
	  
	  for(auto& p : points){
	    p.resize(DIMENSION);
	    
	    for(unsigned int d=0;d<DIMENSION;d++){
	      whiteice::math::blas_real<float> value = rng.uniform()*2.0f - 1.0f; // [-1,1]
	      p[d] = value.c[0];
	    }
	    
	  }
	  
	  startPoint = endPoint;
	  endPoint = points;
	  
	  if(startPoint.size() == 0)
	    startPoint = points;
	  
	  latestTickCurveDrawn = tick;
	}
	
	curveParameter = (tick - latestTickCurveDrawn)/TICKSPERCURVE;
	
	for(unsigned int j=0;j<points.size();j++){
	  points[j].resize(DIMENSION);
	  for(unsigned int d=0;d<DIMENSION;d++){
	    points[j][d] = (1.0 - curveParameter)*startPoint[j][d] + curveParameter*endPoint[j][d];
	  }
	  
	}
      }
      
      createHermiteCurve(curve, points, 0.0, 200);
      
      SDL_Renderer* renderer = SDL_CreateSoftwareRenderer(surface);
      
      if(renderer == NULL)
	return false;
      
      
      SDL_SetRenderDrawColor(renderer, 0xFF, 0xFF, 0xFF, 0xFF);
      
      math::matrix< math::blas_real<double> > R; // rotation matrix
      R.rotation(2*angle1, 2*angle2, 2*angle3);
      
      for(unsigned int i=0;i<curve.size();i++){
	auto p = curve[i];
	
	unsigned int index = i;
	if(index == 0) index = curve.size()-1;
	else index--;
	auto pprev = curve[index];
	
	// rotates points
	
	auto pv = p;
	pv.resize(4);
	pv[0] = p[0]; pv[1] = p[1]; pv[2] = p[2]; pv[3] = 1.0;
	pv = R*pv;
	p[0] = pv[0]; p[1] = pv[1]; p[2] = pv[2];
	
	pv[0] = pprev[0]; pv[1] = pprev[1]; pv[2] = pprev[2]; pv[3] = 1.0;
	pv = R*pv;
	pprev[0] = pv[0]; pprev[1] = pv[1]; pprev[2] = pv[2];
	
	
	double z = 4.0f + p[2].c[0];
	double zp = 4.0f + pprev[2].c[0];
	
	int x = 0;
	const double scalingx = 2.2*SCREEN_WIDTH/4;
	math::convert(x, scalingx*p[0]/z + SCREEN_WIDTH/2);
	
	int y = 0;
	const double scalingy = 2.2*SCREEN_HEIGHT/4;
	math::convert(y, scalingy*p[1]/z + SCREEN_HEIGHT/2);
	
	int xp = 0;
	math::convert(xp, scalingx*pprev[0]/zp + SCREEN_WIDTH/2);
	
	int yp = 0;
	math::convert(yp, scalingy*pprev[1]/zp + SCREEN_HEIGHT/2);
	
	SDL_RenderDrawLine(renderer, xp, yp, x, y);
      }
      
      SDL_DestroyRenderer(renderer);
      
      floodfill(0, 0, surface, 0x20, 0x20, 0x20);
      floodfill(0, SCREEN_HEIGHT-1, surface, 0x20, 0x20, 0x20);
      floodfill(SCREEN_WIDTH-1, 0, surface, 0x20, 0x20, 0x20);
      floodfill(SCREEN_WIDTH-1, SCREEN_HEIGHT-1, surface, 0x20, 0x20, 0x20);
    }

  }

  return true;
}
