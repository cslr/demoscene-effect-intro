/*
 * SDLAVCodec.h
 *
 * libavcodec based video encoding class
 *
 *  Created on: 16.2.2023
 *      Author: Tomas
 */

#ifndef SDLAVCODEC_H_
#define SDLAVCODEC_H_

#include <SDL.h>

extern "C" {
  
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libavutil/hwcontext.h>
  
};

#include <list>
#include <string>
#include <thread>
#include <mutex>

#include <dinrhiw.h>


namespace whiteice {
  namespace resonanz {
    
    /**
     * Class to help encoding (and decoding) SDL_Surface
     * frames into
     *
     */
    class SDLAVCodec {
    public:
      SDLAVCodec(float q = 0.8f); // encoding quality between 0 and 1
      virtual ~SDLAVCodec();
      
      // setups encoding structure
      bool startEncoding(const std::string& filename, unsigned int width, unsigned int height);

      bool setupEncoder(); // helper function..
      
      // inserts SDL_Surface picture frame into video at msecs
      // onwards since the start of the encoding (msecs = 0 is the first frame)
      // [nullptr means black empty frame]
      bool insertFrame(unsigned long long msecs,
		       SDL_Surface* surface = nullptr);
      
      // stops encoding with a final frame [nullptr means black empty frame]
      bool stopEncoding(unsigned long long msecs,
			SDL_Surface* surface = nullptr);

      bool busy() const {
	std::lock_guard<std::mutex> lock1(incoming_mutex);
	bool r = (bool)(((this->incoming).size()) > 0);
	
	return r;
      }
      
      // error was detected during encoding: restart encoding to try again
      bool error() const { return error_flag; }
      
    private:
      bool __insert_frame(unsigned long long msecs, SDL_Surface* surface, bool last);
      
      struct videoframe {
	AVFrame* frame;
	
	// msecs since the start of the [encoded] video
	unsigned long long msecs;
	
	// last frame in video: instructs encoder loop to shutdown after this one
	bool last;
      };
      
      float quality;
      
      const long long FPS; // video frames per second
      const long long MSECS_PER_FRAME;
      long long latest_frame_encoded;
      
      mutable std::mutex incoming_mutex;
      
      SDLAVCodec::videoframe* prev;
      
      const unsigned int MAX_QUEUE_LENGTH = 10000*FPS; // maximum of 1 minute (60 seconds) of frames..
      std::list<SDLAVCodec::videoframe*> incoming; // incoming frames for the encoder (loop)
      
      bool running;
      bool error_flag;
      
      // thread to do all encoding communication between theora and
      // writing resulting frames into disk
      void encoder_loop();
      
      // encodes single video frame
      bool encode_frame(AVFrame* buffer, bool last=false);
      
      int frameHeight, frameWidth; // divisable by 16..

      std::mutex start_lock;
      std::thread* encoder_thread;
      FILE* handle;


      AVFormatContext* fmt_ctx = nullptr;
      AVStream* stream; 
      
      const AVCodec* codec = nullptr;
      AVCodecContext* av_ctx = nullptr;
      
      
      AVFrame *frame = nullptr;
      AVPacket *pkt = nullptr;
      
    };
    
    
  }
} /* namespace whiteice */

#endif /* SDLAVCODEC_H_ */
