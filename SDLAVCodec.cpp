/*
 * SDLAVCodec.cpp
 *
 *  Created on: 16.2.2023
 *      Author: Tomas Ukkonen
 */

#include "SDLAVCodec.h"
#include <string.h>

#include <ogg/ogg.h>
#include <math.h>

extern "C"
{

#include <libavutil/avassert.h>
#include <libavutil/channel_layout.h>
#include <libavutil/opt.h>
#include <libavutil/mathematics.h>
#include <libavutil/timestamp.h>

};

//def av_err2str
#define av_err2str2(errnum) av_make_error_string((char*)__builtin_alloca(AV_ERROR_MAX_STRING_SIZE), AV_ERROR_MAX_STRING_SIZE, errnum)


#include <chrono>
#include <thread>

#include "Log.h"


namespace whiteice {
namespace resonanz {

SDLAVCodec::SDLAVCodec(float q) :
  FPS(100), MSECS_PER_FRAME(1000/100) // currently saves at 25 frames per second, now 100, now 30, now 60
{
  if(q >= 0.0f && q <= 1.0f)
    quality = q;
  else
    quality = 0.5f;
  
  running = false;
  encoder_thread = nullptr;
  error_flag = false;
  
  //av_register_all();
}

  
SDLAVCodec::~SDLAVCodec()
{
  std::lock_guard<std::mutex> lock1(incoming_mutex);
  
  for(auto& i : incoming){
    av_frame_free(&(i->frame));
    delete i;
  }
  
  incoming.clear();
  
  std::lock_guard<std::mutex> lock2(start_lock);
  
  running = false;
  if(encoder_thread){
    encoder_thread->detach();
    delete encoder_thread; // shutdown using force
  }

  if(running){
    // encode_frame(nullptr ,true);
    avcodec_free_context(&av_ctx);
    av_frame_free(&frame);
    av_packet_free(&pkt);

    av_ctx = NULL;
    frame = NULL;
    pkt = NULL;
  }
  
}


// setups encoding structure
bool SDLAVCodec::startEncoding(const std::string& filename,
			       unsigned int width, unsigned int height)
{
  std::lock_guard<std::mutex> lock(start_lock);
  
  if(width <= 0 || height <= 0)
    return false;
  
  if(running)
    return false;
  
  error_flag = false;
  
  frameHeight = height;
  frameWidth = width;

  const char* codec_name = "mpeg4";
  // const char* codec_name = "h264_mf";
  // const char* codec_name = "h264";
  // const char* codec_name = "libx264";
  // const AVCodec *codec;
  int ret;
  
  codec = avcodec_find_encoder_by_name(codec_name);
  if (!codec) {
    fprintf(stderr, "Codec '%s' not found\n", codec_name);
    return false;
  }

#if 1
  avformat_alloc_output_context2(&fmt_ctx,
				 av_guess_format("mp4",
						 filename.c_str(),
						 "video/mp4"),
				 NULL,
				 filename.c_str());
  if(fmt_ctx == NULL)
    return false;


  stream = avformat_new_stream(fmt_ctx, NULL);
  if(stream == NULL) return false;

  //printf("NUMBER OF STREAMS: %d\n", fmt_ctx->nb_streams);
  
#endif

  av_ctx = avcodec_alloc_context3(codec);
  if (!av_ctx) {
    fprintf(stderr, "Could not allocate video codec context\n");
    return false;
  }


  //stream->index = 0;

  /* put sample parameters */
  av_ctx->bit_rate = frameWidth * frameHeight * FPS * 2;
  /* resolution must be a multiple of two */
  av_ctx->width = frameWidth;
  av_ctx->height = frameHeight;
  /* frames per second */
  av_ctx->time_base = (AVRational){1, (int)FPS};
  av_ctx->framerate = (AVRational){(int)FPS, 1};

  // stream->time_base = av_ctx->time_base;
  
    /* emit one intra frame every ten frames
     * check frame pict_type before passing frame
     * to encoder, if frame->pict_type is AV_PICTURE_TYPE_I
     * then gop_size is ignored and the output of encoder
     * will always be I frame irrespective to gop_size
     */
  
  av_ctx->gop_size = 10;
  av_ctx->max_b_frames = 1;
  av_ctx->pix_fmt = AV_PIX_FMT_YUV420P;

#if 1
  if (fmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
    av_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
#endif
 
  //if (codec->id == AV_CODEC_ID_H264){
  {
    av_opt_set(av_ctx->priv_data, "preset", "ultrafast", 0); // "slow"
    
    // "quality of compression" (default: 23), 0 is lossless, 1 is high-quality, 32 is ok
    av_opt_set(av_ctx->priv_data, "crf", "32", 0);
  }
  
  /* open it */
  ret = avcodec_open2(av_ctx, codec, NULL);
  if (ret < 0) {
    fprintf(stderr, "Could not open codec: %s\n", av_err2str2(ret));
    return false;
  }

#if 1
  ret = avcodec_parameters_from_context(stream->codecpar, av_ctx);
  if(ret < 0) return false;
#endif

  stream->start_time = 0;

  stream->time_base = (AVRational){1, (int)FPS};
  stream->avg_frame_rate = (AVRational){(int)FPS, 1};
  stream->r_frame_rate = (AVRational){(int)FPS, 1};
  
  stream->id = fmt_ctx->nb_streams - 1;
  
  // stream->codec = av_ctx;
  
  // printf("CODEC: %d %d\n", AV_CODEC_ID_H264, codec->id);
	 

  fmt_ctx->start_time = 0;
  fmt_ctx->video_codec = (AVCodec*)codec;
  // fmt_ctx->oformat->name = "mp4";
  // fmt_ctx->oformat->video_codec = codec->id; // AV_CODEC_ID_H264;
  fmt_ctx->video_codec_id = codec->id;
  fmt_ctx->bit_rate = frameWidth * frameHeight * FPS * 2;
  

  // printf("VIDEO FORMAT:\n");
  av_dump_format(fmt_ctx, 0, filename.c_str(), 1);

#if 1
  ret = avio_open(&fmt_ctx->pb, filename.c_str(), AVIO_FLAG_WRITE);
  if(ret < 0) return false;
  
  /* init muxer, write output file header */
  ret = avformat_write_header(fmt_ctx, NULL);
  if (ret < 0) {
    printf("Error occurred when opening output file\n");
    return false;
  }
#endif
  

  handle = NULL;
  // handle = fopen(filename.c_str(), "w");
  

  frame = av_frame_alloc();
  if (!frame) {
    fprintf(stderr, "Could not allocate video frame\n");
    return false;
  }
  frame->format = av_ctx->pix_fmt;
  frame->width  = av_ctx->width;
  frame->height = av_ctx->height;

  pkt = av_packet_alloc();
  if (!pkt) return false;
  
  
  try{
    latest_frame_encoded = -1;
    encoder_thread = new std::thread(&SDLAVCodec::encoder_loop, this);
    
    if(encoder_thread == nullptr){
      running = false;
      return false;
    }
  }
  catch(std::exception& e){
    running = false;
    return false;
  }
  
  return true;
}


// inserts SDL_Surface picture frame into video at msecs
// onwards since the start of the encoding (msecs = 0 is the first frame)
bool SDLAVCodec::insertFrame(unsigned long long msecs, SDL_Surface* surface)
{
  // very quick skipping of frames [without conversion] when picture for the current frame has been already inserted
  const unsigned long long frame = msecs/MSECS_PER_FRAME;
  if((signed)frame <= latest_frame_encoded)
    return false;
  
  if(running){
    if(__insert_frame(msecs, surface, false)){
      latest_frame_encoded = frame;
      return true;
    }
		else{
		  return false;
		}
  }
  else{
    return false;
  }
}

// inserts last frame and stops encoding (and saves and closes file when encoding has stopped)
bool SDLAVCodec::stopEncoding(unsigned long long msecs,
			      SDL_Surface* surface)
{
  if(running){
    if(__insert_frame(msecs, surface, true) == false){
      logging.fatal("sdl-theora: inserting LAST frame failed");
      return false;
    }
  }
  else{
    logging.fatal("sdl-theora: not running and calling stopEncoding()");
    return false;
  }

  std::lock_guard<std::mutex> lock(start_lock);

  while(this->busy()) // waits for encoding to finish..
    std::this_thread::sleep_for(std::chrono::milliseconds(MSECS_PER_FRAME/10)); 

  running = false;
  
  if(encoder_thread){
    // may block forever... [should do some kind of timed wait instead?]
    encoder_thread->join();
    delete encoder_thread;
  }
  encoder_thread = nullptr;

  // encode_frame(nullptr, true);


  uint8_t endcode[] = { 0, 0, 1, 0xb7 };
  
  if(handle){
    if (codec->id == AV_CODEC_ID_MPEG1VIDEO ||
	codec->id == AV_CODEC_ID_MPEG2VIDEO)
      fwrite(endcode, 1, sizeof(endcode), handle);
    
    fclose(handle);
  }
  handle = NULL;
  

  av_write_trailer(fmt_ctx);
  
  avcodec_free_context(&av_ctx);
  av_frame_free(&frame);
  av_packet_free(&pkt);
  av_free(stream);

  av_ctx = NULL;
  frame = NULL;
  pkt = NULL;
  
  running = false; // it is safe to do because we have start lock?
  
  return true; // everything went correctly
}


bool SDLAVCodec::__insert_frame(unsigned long long msecs, SDL_Surface* surface, bool last)
{
  // converts SDL into YUV format [each plane separatedly and have full width and height]
  // before sending it to the encoder thread
  
  SDL_Surface* frame = SDL_CreateRGBSurface(0, frameWidth, frameHeight, 32, 0x00FF0000, 0x0000FF00, 0x000000FF, 0);
  
  if(frame == NULL){
    logging.error("sdl-theora::__insert_frame failed [1]");
    return false;
  }
  
  if(surface != NULL){
    SDL_BlitSurface(surface, NULL, frame, NULL);
  }
  else{ // just fills the frame with black
    SDL_FillRect(frame, NULL, SDL_MapRGB(frame->format, 0, 0, 0));
  }
  
  // assumes yuv pixels format is full plane for each component:
  // Y plane (one byte per pixel), U plane (one byte per pixel), V plane (one byte per pixel)
  
  SDLAVCodec::videoframe* f = new SDLAVCodec::videoframe;
  
  f->msecs = msecs;
  
  f->frame = av_frame_alloc();
  
  f->frame->format = av_ctx->pix_fmt;
  f->frame->width = frameWidth;
  f->frame->height = frameHeight;

  const long long f_frame = (f->msecs / MSECS_PER_FRAME);
  f->frame->pts = f_frame;

  if(av_frame_get_buffer(f->frame, 0) != 0){
    error_flag = true;
    printf("ERROR\n");
  }
  
  if(av_frame_make_writable(f->frame) != 0){
    error_flag = true;
    printf("ERROR\n");
  }

  /*
  printf("FRAMEDATA: %llx %llx %llx %llx PTS: %ld\n",
	 (unsigned long long)(f->frame),
	 (unsigned long long)(f->frame->data[0]),
	 (unsigned long long)(f->frame->linesize[0]),
	 (unsigned long long)(f),
	 f->frame->pts);
  */
  
  
  // perfect opportunity for parallelization: pixel conversions are independet from each other
#pragma omp parallel for
  for(int y=0; y<f->frame->height; y++) {
    for(int x=0; x<f->frame->width; x++) {
      
      const unsigned int index = x + frameWidth*y;
      unsigned int* source = (unsigned int*)(frame->pixels);
      
      const unsigned int r = (source[index] & 0x00FF0000)>>16;
      const unsigned int g = (source[index] & 0x0000FF00)>> 8;
      const unsigned int b = (source[index] & 0x000000FF)>> 0;
      
      auto Y  =  (0.257*r) + (0.504*g) + (0.098*b) + 16.0;
      auto Cr =  (0.439*r) - (0.368*g) - (0.071*b) + 128.0;
      auto Cb = -(0.148*r) - (0.291*g) + (0.439*b) + 128.0;
      
      if(Y < 0.0) Y = 0.0;
      else if(Y > 255.0) Y = 255.0;
      
      if(Cr < 0.0) Cr = 0.0;
      else if(Cr > 255.0) Cr = 255.0;
      
      if(Cb < 0.0) Cb = 0.0;
      else if(Cb > 255.0) Cb = 255.0;
      
      f->frame->data[0][y * f->frame->linesize[0] + x] =
	(unsigned char)round(Y);  // Y
      //f->frame->data[1][y * f->frame->linesize[1] + x] =
      //(unsigned char)round(Cb);  // Cb
      //f->frame->data[2][y * f->frame->linesize[2] + x] =
      //(unsigned char)round(Cr);  // Cr
    }
  }


#pragma omp parallel for
  for(int y=0; y<f->frame->height; y++) {
    for(int x=0; x<f->frame->width; x++) {
      
      const unsigned int index = x + frameWidth*y;
      unsigned int* source = (unsigned int*)(frame->pixels);
      
      const unsigned int r = (source[index] & 0x00FF0000)>>16;
      const unsigned int g = (source[index] & 0x0000FF00)>> 8;
      const unsigned int b = (source[index] & 0x000000FF)>> 0;
      
      auto Y  =  (0.257*r) + (0.504*g) + (0.098*b) + 16.0;
      auto Cr =  (0.439*r) - (0.368*g) - (0.071*b) + 128.0;
      auto Cb = -(0.148*r) - (0.291*g) + (0.439*b) + 128.0;
      
      if(Y < 0.0) Y = 0.0;
      else if(Y > 255.0) Y = 255.0;
      
      if(Cr < 0.0) Cr = 0.0;
      else if(Cr > 255.0) Cr = 255.0;
      
      if(Cb < 0.0) Cb = 0.0;
      else if(Cb > 255.0) Cb = 255.0;
      
      //f->frame->data[0][y * f->frame->linesize[0] + x] =
      //(unsigned char)round(Y);  // Y

      int xx = x/2;
      int yy = y/2;
      
      f->frame->data[1][yy * f->frame->linesize[1] + xx] =
	(unsigned char)round(Cb);  // Cb
      f->frame->data[2][yy * f->frame->linesize[2] + xx] =
	(unsigned char)round(Cr);  // Cr
    }
  }
  
  
  f->last = last; // IMPORTANT!
  
  {
    std::lock_guard<std::mutex> lock1(start_lock);
    std::lock_guard<std::mutex> lock2(incoming_mutex);
    
    // always processes special LAST frames
    if((running == false || incoming.size() >= MAX_QUEUE_LENGTH) && f->last != true){
      logging.error("sdl-theora::__insert_frame failed [3]");

      av_frame_free(&(f->frame));
      delete f;

      SDL_FreeSurface(frame);
      return false;
    }
    else
      incoming.push_back(f);
  }
  
  SDL_FreeSurface(frame);
  
  return true;
}


// thread to do all encoding communication between theora and
// writing resulting frames into disk
void SDLAVCodec::encoder_loop()
{
  running = true;
  
  logging.info("sdl-theora: encoder thread started..");
  
  prev = nullptr;
  
  logging.info("sdl-theora: theora video headers written..");
  
  // keeps encoding incoming frames
  SDLAVCodec::videoframe* f = nullptr;
  int latest_frame_generated = -1;
  prev = nullptr;
  
  while(1)
  {
    {
      incoming_mutex.lock();

      {
	char buffer[80];
	snprintf(buffer, 80, "sdl-theora: incoming frame buffer size: %d", (int)incoming.size());
	logging.info(buffer);
      }
      
      if(incoming.size() > 0){ // has incoming picture data
	f = incoming.front();
	incoming.pop_front();
	incoming_mutex.unlock();
      }
      else{
	incoming_mutex.unlock();
	
	// sleep here ~10ms [time between frames 1ms]
	std::this_thread::sleep_for(std::chrono::milliseconds(MSECS_PER_FRAME/10));
	
	continue;
      }
    }
    
    // converts milliseconds field to frame number
    long long f_frame = (f->msecs / MSECS_PER_FRAME);
    
    // if there has been no frames between:
    // last_frame_generated .. f_frame
    // fills them with latest_frame_generated (prev)
    
    if(latest_frame_generated < 0 && f_frame >= 0){
      // writes f frame
      latest_frame_generated = 0;
      
      logging.info("sdl-theora: writing initial f-frames");

      for(long long i = latest_frame_generated;i<f_frame;i++){
	f->frame->pts = i;
	
	if(encode_frame(f->frame) == false)
	  logging.error("sdl-theora: encoding frame failed");
	else{
	  char buffer[80];
	  snprintf(buffer, 80, "sdl-theora: encoding frame: %lld/%lld", i, FPS);
	  logging.info(buffer);
	}
	
      }

      latest_frame_generated = f_frame;
    }
    else if((latest_frame_generated+1) < f_frame){
      // writes prev frames
      
      logging.info("sdl-theora: writing prev-frames");
      
      for(long long i=(latest_frame_generated+1);i<(f_frame-1);i++){
	prev->frame->pts = i;

	if(encode_frame(prev->frame) == false)
	  logging.error("sdl-theora: encoding prev-frame failed");
	else{
	  char buffer[80];
	  snprintf(buffer, 80, "sdl-theora: encoding prev-frame: %lld/%lld", i, FPS);
	  logging.info(buffer);
	}
	
      }

      latest_frame_generated = f_frame - 1;
    }
    
    // writes f-frame once (f_frame) if it is a new frame for this msec
    // OR if it is a last frame [stream close frame]
    if(latest_frame_generated < f_frame || f->last)
    {
      logging.info("sdl-theora: writing current frame");

      f->frame->pts = f_frame;
      
      if(encode_frame(f->frame, f->last) == false)
	logging.error("sdl-theora: encoding frame failed");
      else{
	char buffer[80];
	snprintf(buffer, 80, "sdl-theora: encoding frame: %lld/%lld", f_frame, FPS);
	logging.info(buffer);
      }
      
    }
    
    latest_frame_generated = f_frame;

    if(prev != nullptr){

      av_frame_free(&(prev->frame));
      delete prev;
      prev = nullptr;
    }
    
    prev = f;
    
    if(f->last == true){
      logging.info("sdl-theora: special last frame seen => exit");
      break;
    }
  }
  
  logging.info("sdl-theora: theora encoder thread shutdown sequence..");
  
  logging.info("sdl-theora: encoder thread shutdown: ogg_stream_destroy");
  logging.info("sdl-theora: encoder thread shutdown: ogg_stream_destroy.. done");

  
  // all frames has been written
  if(prev != nullptr){
    av_frame_free(&(prev->frame));
    delete prev;
    prev = nullptr;
  }
  
  logging.info("sdl-theora: encoder thread shutdown: incoming buffer clear");
  
  {
    std::lock_guard<std::mutex> lock1(incoming_mutex);
    for(auto i : incoming){
      av_frame_free(&(i->frame));
      delete i;
    }
    
    incoming.clear();
  }
  
  {
    logging.info("sdl-theora: encoder thread halt. running = false");
    running = false;
  }
  
}


bool SDLAVCodec::encode_frame(AVFrame* buffer,
			      bool last)
{
  if(avcodec_send_frame(av_ctx, buffer) < 0)
    return false;
  
  AVPacket packet;
  av_init_packet(&packet);
  
  int ret = 0;
  
  while(ret >= 0) {
    ret = avcodec_receive_packet(av_ctx, &packet);
    if(ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
      return true;  // nothing to write
    }
    if(ret < 0) return false;
    // assert(ret >= 0);

    // av_packet_rescale_ts(&packet, av_ctx->time_base, av_ctx->time_base);


    // packet.stream_index = stream->id;
    packet.pts = buffer->pts;
    packet.dts = buffer->pts;

#if 0
    printf("STREAMS:\n");
    printf("PACKET STREAM: %d\n", packet.stream_index);
    for(unsigned int i=0;i<fmt_ctx->nb_streams;i++){
      printf("%d: %llx\n", i, (unsigned long long)fmt_ctx->streams[i]);
    }
#endif

#if 1
    av_packet_rescale_ts(&packet,
			 av_ctx->time_base, // your theoric timebase
			 fmt_ctx->streams[packet.stream_index]->time_base); // the actual timebase
#endif
    
    
    //fwrite(packet.data, 1, packet.size, handle);
    av_write_frame(fmt_ctx, &packet);
    
    av_packet_unref(&packet);
  }
	  
  return true;
}



}
} /* namespace whiteice */
