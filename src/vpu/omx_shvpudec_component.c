/**
  @file omx_shvpudec_component.c
  
  This component implements H.264 / MPEG-4 AVC video decoder. 
  The H.264 / MPEG-4 AVC Video decoder is based on libshcodecs.

  Copyright (C) 2008      Renesas Solutions

  Adapted from the Bellagio libomxil ffmpeg videodec component,

  Copyright (C) 2007-2008 STMicroelectronics
  Copyright (C) 2007-2008 Nokia Corporation and/or its subsidiary(-ies)

  This library is free software; you can redistribute it and/or modify it under
  the terms of the GNU Lesser General Public License as published by the Free
  Software Foundation; either version 2.1 of the License, or (at your option)
  any later version.

  This library is distributed in the hope that it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
  FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more
  details.

  You should have received a copy of the GNU Lesser General Public License
  along with this library; if not, write to the Free Software Foundation, Inc.,
  51 Franklin St, Fifth Floor, Boston, MA
  02110-1301  USA

  $Date$
  Revision $Rev$
  Author $Author$
*/

#include <omxcore.h>
#include <omx_base_video_port.h>
#include<OMX_Video.h>

#include "omx_shvpudec_component.h"

/** Maximum Number of Video Component Instance*/
#define MAX_COMPONENT_VIDEODEC 4

/** Counter of Video Component Instance*/
static OMX_U32 noVideoDecInstance = 0;

/** The output decoded color format */
#define OUTPUT_DECODED_COLOR_FMT OMX_COLOR_FormatYUV420Planar

#define DEFAULT_WIDTH 352   
#define DEFAULT_HEIGHT 288   
/** define the max input buffer size */   
#define DEFAULT_VIDEO_OUTPUT_BUF_SIZE DEFAULT_WIDTH*DEFAULT_HEIGHT*3/2   // YUV 420P 

/** The Constructor of the video decoder component
  * @param openmaxStandComp the component handle to be constructed
  * @param cComponentName is the name of the constructed component
  */
OMX_ERRORTYPE omx_shvpudec_component_Constructor(OMX_COMPONENTTYPE *openmaxStandComp,OMX_STRING cComponentName) {

  OMX_ERRORTYPE eError = OMX_ErrorNone;  
  omx_shvpudec_component_PrivateType* omx_shvpudec_component_Private;
  omx_base_video_PortType *inPort,*outPort;
  OMX_U32 i;

  if (!openmaxStandComp->pComponentPrivate) {
    DEBUG(DEB_LEV_FUNCTION_NAME, "In %s, allocating component\n", __func__);
    openmaxStandComp->pComponentPrivate = calloc(1, sizeof(omx_shvpudec_component_PrivateType));
    if(openmaxStandComp->pComponentPrivate == NULL) {
      return OMX_ErrorInsufficientResources;
    }
  } else {
    DEBUG(DEB_LEV_FUNCTION_NAME, "In %s, Error Component %x Already Allocated\n", __func__, (int)openmaxStandComp->pComponentPrivate);
  }

  omx_shvpudec_component_Private = openmaxStandComp->pComponentPrivate;
  omx_shvpudec_component_Private->ports = NULL;

  eError = omx_base_filter_Constructor(openmaxStandComp, cComponentName);

  omx_shvpudec_component_Private->sPortTypesParam[OMX_PortDomainVideo].nStartPortNumber = 0;
  omx_shvpudec_component_Private->sPortTypesParam[OMX_PortDomainVideo].nPorts = 2;

  /** Allocate Ports and call port constructor. */
  if (omx_shvpudec_component_Private->sPortTypesParam[OMX_PortDomainVideo].nPorts && !omx_shvpudec_component_Private->ports) {
    omx_shvpudec_component_Private->ports = calloc(omx_shvpudec_component_Private->sPortTypesParam[OMX_PortDomainVideo].nPorts, sizeof(omx_base_PortType *));
    if (!omx_shvpudec_component_Private->ports) {
      return OMX_ErrorInsufficientResources;
    }
    for (i=0; i < omx_shvpudec_component_Private->sPortTypesParam[OMX_PortDomainVideo].nPorts; i++) {
      omx_shvpudec_component_Private->ports[i] = calloc(1, sizeof(omx_base_video_PortType));
      if (!omx_shvpudec_component_Private->ports[i]) {
        return OMX_ErrorInsufficientResources;
      }
    }
  }

  base_video_port_Constructor(openmaxStandComp, &omx_shvpudec_component_Private->ports[0], 0, OMX_TRUE);
  base_video_port_Constructor(openmaxStandComp, &omx_shvpudec_component_Private->ports[1], 1, OMX_FALSE);

  /** here we can override whatever defaults the base_component constructor set
    * e.g. we can override the function pointers in the private struct  
    */

  /** Domain specific section for the ports.   
    * first we set the parameter common to both formats
    */
  //common parameters related to input port
  inPort = (omx_base_video_PortType *)omx_shvpudec_component_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX];
  inPort->sPortParam.nBufferSize = DEFAULT_OUT_BUFFER_SIZE;
  inPort->sPortParam.format.video.xFramerate = 25;

  //common parameters related to output port
  outPort = (omx_base_video_PortType *)omx_shvpudec_component_Private->ports[OMX_BASE_FILTER_OUTPUTPORT_INDEX];
  outPort->sPortParam.format.video.eColorFormat = OUTPUT_DECODED_COLOR_FMT;
  outPort->sPortParam.nBufferSize = DEFAULT_VIDEO_OUTPUT_BUF_SIZE;
  outPort->sPortParam.format.video.xFramerate = 25;

  /** settings of output port parameter definition */
  outPort->sVideoParam.eColorFormat = OUTPUT_DECODED_COLOR_FMT;
  outPort->sVideoParam.xFramerate = 25;

  /** now it's time to know the video coding type of the component */
  if(!strcmp(cComponentName, VIDEO_DEC_MPEG4_NAME)) { 
    omx_shvpudec_component_Private->video_coding_type = OMX_VIDEO_CodingMPEG4;
  } else if(!strcmp(cComponentName, VIDEO_DEC_H264_NAME)) { 
    omx_shvpudec_component_Private->video_coding_type = OMX_VIDEO_CodingAVC;
  } else if (!strcmp(cComponentName, VIDEO_DEC_BASE_NAME)) {
    omx_shvpudec_component_Private->video_coding_type = OMX_VIDEO_CodingUnused;
  } else {
    // IL client specified an invalid component name 
    return OMX_ErrorInvalidComponentName;
  }  

  if(!omx_shvpudec_component_Private->avCodecSyncSem) {
    omx_shvpudec_component_Private->avCodecSyncSem = calloc(1,sizeof(tsem_t));
    if(omx_shvpudec_component_Private->avCodecSyncSem == NULL) {
      return OMX_ErrorInsufficientResources;
    }
    tsem_init(omx_shvpudec_component_Private->avCodecSyncSem, 0);
  }

  SetInternalVideoParameters(openmaxStandComp);

  //omx_shvpudec_component_Private->eOutFramePixFmt = PIX_FMT_YUV420P;

  if(omx_shvpudec_component_Private->video_coding_type == OMX_VIDEO_CodingMPEG4) {
    omx_shvpudec_component_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX]->sPortParam.format.video.eCompressionFormat = OMX_VIDEO_CodingMPEG4;
  } else {
    omx_shvpudec_component_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX]->sPortParam.format.video.eCompressionFormat = OMX_VIDEO_CodingAVC;
  }

  /** general configuration irrespective of any video formats
    * setting other parameters of omx_shvpudec_component_private  
    */
  omx_shvpudec_component_Private->decoder = NULL;
  omx_shvpudec_component_Private->avcodecReady = OMX_FALSE;
  //omx_shvpudec_component_Private->extradata = NULL;
  //omx_shvpudec_component_Private->extradata_size = 0;
  omx_shvpudec_component_Private->BufferMgmtCallback = omx_shvpudec_component_BufferMgmtCallback;

  /** initializing the codec context etc that was done earlier by ffmpeglibinit function */
  omx_shvpudec_component_Private->messageHandler = omx_shvpudec_component_MessageHandler;
  omx_shvpudec_component_Private->destructor = omx_shvpudec_component_Destructor;
  openmaxStandComp->SetParameter = omx_shvpudec_component_SetParameter;
  openmaxStandComp->GetParameter = omx_shvpudec_component_GetParameter;
  openmaxStandComp->SetConfig    = omx_shvpudec_component_SetConfig;
  openmaxStandComp->ComponentRoleEnum = omx_shvpudec_component_ComponentRoleEnum;
  openmaxStandComp->GetExtensionIndex = omx_shvpudec_component_GetExtensionIndex;

  noVideoDecInstance++;

  if(noVideoDecInstance > MAX_COMPONENT_VIDEODEC) {
    return OMX_ErrorInsufficientResources;
  }
  return eError;
}


/** The destructor of the video decoder component
  */
OMX_ERRORTYPE omx_shvpudec_component_Destructor(OMX_COMPONENTTYPE *openmaxStandComp) {
  omx_shvpudec_component_PrivateType* omx_shvpudec_component_Private = openmaxStandComp->pComponentPrivate;
  OMX_U32 i;
  
#if 0
  if(omx_shvpudec_component_Private->extradata) {
    free(omx_shvpudec_component_Private->extradata);
    omx_shvpudec_component_Private->extradata=NULL;
  }
#endif

  if(omx_shvpudec_component_Private->avCodecSyncSem) {
    tsem_deinit(omx_shvpudec_component_Private->avCodecSyncSem); 
    free(omx_shvpudec_component_Private->avCodecSyncSem);
    omx_shvpudec_component_Private->avCodecSyncSem = NULL;
  }

  /* frees port/s */   
  if (omx_shvpudec_component_Private->ports) {   
    for (i=0; i < omx_shvpudec_component_Private->sPortTypesParam[OMX_PortDomainVideo].nPorts; i++) {   
      if(omx_shvpudec_component_Private->ports[i])   
        omx_shvpudec_component_Private->ports[i]->PortDestructor(omx_shvpudec_component_Private->ports[i]);   
    }   
    free(omx_shvpudec_component_Private->ports);   
    omx_shvpudec_component_Private->ports=NULL;   
  } 


  DEBUG(DEB_LEV_FUNCTION_NAME, "Destructor of video decoder component is called\n");

  omx_base_filter_Destructor(openmaxStandComp);
  noVideoDecInstance--;

  return OMX_ErrorNone;
}


/** Initialize libshcodecs, and open a decoder for the format specified by IL client 
  */ 
OMX_ERRORTYPE omx_shvpudec_component_ffmpegLibInit(omx_shvpudec_component_PrivateType* omx_shvpudec_component_Private) {

  //OMX_U32 target_codecID;  
  SHCodecs_Format format;
  omx_base_video_PortType *inPort;

#if 0
  avcodec_init();
  av_register_all();

  DEBUG(DEB_LEV_SIMPLE_SEQ, "FFmpeg library/codec initialized\n");
#endif

  inPort = (omx_base_video_PortType *)omx_shvpudec_component_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX];

  switch(omx_shvpudec_component_Private->video_coding_type) {
    case OMX_VIDEO_CodingMPEG4 :
      //target_codecID = CODEC_ID_MPEG4;
      format = SHCodecs_Format_MPEG4;
      break;
    case OMX_VIDEO_CodingAVC : 
      //target_codecID = CODEC_ID_H264;
      format = SHCodecs_Format_H264;
      break;
    default :
      DEBUG(DEB_LEV_ERR, "\n codecs other than H.264 / MPEG-4 AVC are not supported -- codec not found\n");
      return OMX_ErrorComponentNotFound;
  }

#if 0
  /** Find the  decoder corresponding to the video type specified by IL client*/
  omx_shvpudec_component_Private->avCodec = avcodec_find_decoder(target_codecID);
  if (omx_shvpudec_component_Private->avCodec == NULL) {
    DEBUG(DEB_LEV_ERR, "Codec not found\n");
    return OMX_ErrorInsufficientResources;
  }
#endif

  /** Initialize the VPU4 decoder */
  omx_shvpudec_component_Private->decoder =
    shcodecs_decoder_init (inPort->sPortParam.format.video.nFrameWidth,
                           inPort->sPortParam.format.video.nFrameHeight,
                           format);
  if (omx_shvpudec_component_Private->decoder == NULL) {
    DEBUG(DEB_LEV_ERR, "Unable to initialize VPU4 decoder\n");
    return OMX_ErrorInsufficientResources;
  }

  //omx_shvpudec_component_Private->avCodecContext = avcodec_alloc_context();

#if 0
  /** necessary flags for MPEG-4 or H.264 stream */
   omx_shvpudec_component_Private->avFrame = avcodec_alloc_frame();
  if(omx_shvpudec_component_Private->extradata_size >0) {
    omx_shvpudec_component_Private->avCodecContext->extradata = omx_shvpudec_component_Private->extradata;
    omx_shvpudec_component_Private->avCodecContext->extradata_size = (int)omx_shvpudec_component_Private->extradata_size;
  } else {
    omx_shvpudec_component_Private->avCodecContext->flags |= CODEC_FLAG_TRUNCATED;
  }
#endif

#if 0
  if (avcodec_open(omx_shvpudec_component_Private->avCodecContext, omx_shvpudec_component_Private->avCodec) < 0) {
    DEBUG(DEB_LEV_ERR, "Could not open codec\n");
    return OMX_ErrorInsufficientResources;
  }
#endif
  tsem_up(omx_shvpudec_component_Private->avCodecSyncSem);

  DEBUG(DEB_LEV_SIMPLE_SEQ, "done\n");

  return OMX_ErrorNone;
}

/** It Deinitializates the ffmpeg framework, and close the ffmpeg video decoder of selected coding type
  */
void omx_shvpudec_component_ffmpegLibDeInit(omx_shvpudec_component_PrivateType* omx_shvpudec_component_Private) {

  shcodecs_decoder_close (omx_shvpudec_component_Private->decoder);

#if 0
  if (omx_shvpudec_component_Private->avCodecContext->priv_data) {
    avcodec_close (omx_shvpudec_component_Private->avCodecContext);
  }
  if (omx_shvpudec_component_Private->extradata_size == 0 && omx_shvpudec_component_Private->avCodecContext->extradata) {
    av_free (omx_shvpudec_component_Private->avCodecContext->extradata);
    //omx_shvpudec_component_Private->avCodecContext->extradata = NULL;
    //omx_shvpudec_component_Private->avCodecContext->extradata_size = 0;
  }
  av_free (omx_shvpudec_component_Private->avCodecContext);

  av_free(omx_shvpudec_component_Private->avFrame);
#endif

}

/** internal function to set codec related parameters in the private type structure 
  */
void SetInternalVideoParameters(OMX_COMPONENTTYPE *openmaxStandComp) {

  omx_shvpudec_component_PrivateType* omx_shvpudec_component_Private;
  omx_base_video_PortType *inPort ; 

  omx_shvpudec_component_Private = openmaxStandComp->pComponentPrivate;;

  if (omx_shvpudec_component_Private->video_coding_type == OMX_VIDEO_CodingMPEG4) {
    strcpy(omx_shvpudec_component_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX]->sPortParam.format.video.cMIMEType,"video/mpeg4");
    omx_shvpudec_component_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX]->sPortParam.format.video.eCompressionFormat = OMX_VIDEO_CodingMPEG4;

    setHeader(&omx_shvpudec_component_Private->pVideoMpeg4, sizeof(OMX_VIDEO_PARAM_MPEG4TYPE));    
    omx_shvpudec_component_Private->pVideoMpeg4.nPortIndex = 0;                                                                    
    omx_shvpudec_component_Private->pVideoMpeg4.nSliceHeaderSpacing = 0;
    omx_shvpudec_component_Private->pVideoMpeg4.bSVH = OMX_FALSE;
    omx_shvpudec_component_Private->pVideoMpeg4.bGov = OMX_FALSE;
    omx_shvpudec_component_Private->pVideoMpeg4.nPFrames = 0;
    omx_shvpudec_component_Private->pVideoMpeg4.nBFrames = 0;
    omx_shvpudec_component_Private->pVideoMpeg4.nIDCVLCThreshold = 0;
    omx_shvpudec_component_Private->pVideoMpeg4.bACPred = OMX_FALSE;
    omx_shvpudec_component_Private->pVideoMpeg4.nMaxPacketSize = 0;
    omx_shvpudec_component_Private->pVideoMpeg4.nTimeIncRes = 0;
    omx_shvpudec_component_Private->pVideoMpeg4.eProfile = OMX_VIDEO_MPEG4ProfileSimple;
    omx_shvpudec_component_Private->pVideoMpeg4.eLevel = OMX_VIDEO_MPEG4Level0;
    omx_shvpudec_component_Private->pVideoMpeg4.nAllowedPictureTypes = 0;
    omx_shvpudec_component_Private->pVideoMpeg4.nHeaderExtension = 0;
    omx_shvpudec_component_Private->pVideoMpeg4.bReversibleVLC = OMX_FALSE;

    inPort = (omx_base_video_PortType *)omx_shvpudec_component_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX];
    inPort->sVideoParam.eCompressionFormat = OMX_VIDEO_CodingMPEG4;

  } else if (omx_shvpudec_component_Private->video_coding_type == OMX_VIDEO_CodingAVC) {
    strcpy(omx_shvpudec_component_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX]->sPortParam.format.video.cMIMEType,"video/avc(h264)");
    omx_shvpudec_component_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX]->sPortParam.format.video.eCompressionFormat = OMX_VIDEO_CodingAVC;

    setHeader(&omx_shvpudec_component_Private->pVideoAvc, sizeof(OMX_VIDEO_PARAM_AVCTYPE));
    omx_shvpudec_component_Private->pVideoAvc.nPortIndex = 0;
    omx_shvpudec_component_Private->pVideoAvc.nSliceHeaderSpacing = 0;
    omx_shvpudec_component_Private->pVideoAvc.bUseHadamard = OMX_FALSE;
    omx_shvpudec_component_Private->pVideoAvc.nRefFrames = 2;
    omx_shvpudec_component_Private->pVideoAvc.nPFrames = 0;
    omx_shvpudec_component_Private->pVideoAvc.nBFrames = 0;
    omx_shvpudec_component_Private->pVideoAvc.bUseHadamard = OMX_FALSE;
    omx_shvpudec_component_Private->pVideoAvc.nRefFrames = 2;
    omx_shvpudec_component_Private->pVideoAvc.eProfile = OMX_VIDEO_AVCProfileBaseline;
    omx_shvpudec_component_Private->pVideoAvc.eLevel = OMX_VIDEO_AVCLevel1;
    omx_shvpudec_component_Private->pVideoAvc.nAllowedPictureTypes = 0;
    omx_shvpudec_component_Private->pVideoAvc.bFrameMBsOnly = OMX_FALSE;
    omx_shvpudec_component_Private->pVideoAvc.nRefIdx10ActiveMinus1 = 0;
    omx_shvpudec_component_Private->pVideoAvc.nRefIdx11ActiveMinus1 = 0;
    omx_shvpudec_component_Private->pVideoAvc.bEnableUEP = OMX_FALSE;  
    omx_shvpudec_component_Private->pVideoAvc.bEnableFMO = OMX_FALSE;  
    omx_shvpudec_component_Private->pVideoAvc.bEnableASO = OMX_FALSE;  
    omx_shvpudec_component_Private->pVideoAvc.bEnableRS = OMX_FALSE;   

    omx_shvpudec_component_Private->pVideoAvc.bMBAFF = OMX_FALSE;               
    omx_shvpudec_component_Private->pVideoAvc.bEntropyCodingCABAC = OMX_FALSE;  
    omx_shvpudec_component_Private->pVideoAvc.bWeightedPPrediction = OMX_FALSE; 
    omx_shvpudec_component_Private->pVideoAvc.nWeightedBipredicitonMode = 0; 
    omx_shvpudec_component_Private->pVideoAvc.bconstIpred = OMX_FALSE;
    omx_shvpudec_component_Private->pVideoAvc.bDirect8x8Inference = OMX_FALSE;  
    omx_shvpudec_component_Private->pVideoAvc.bDirectSpatialTemporal = OMX_FALSE;
    omx_shvpudec_component_Private->pVideoAvc.nCabacInitIdc = 0;
    omx_shvpudec_component_Private->pVideoAvc.eLoopFilterMode = OMX_VIDEO_AVCLoopFilterDisable;

    inPort = (omx_base_video_PortType *)omx_shvpudec_component_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX];
    inPort->sVideoParam.eCompressionFormat = OMX_VIDEO_CodingAVC;
  }
}


/** The Initialization function of the video decoder
  */
OMX_ERRORTYPE omx_shvpudec_component_Init(OMX_COMPONENTTYPE *openmaxStandComp) {

  omx_shvpudec_component_PrivateType* omx_shvpudec_component_Private = openmaxStandComp->pComponentPrivate;
  OMX_ERRORTYPE eError = OMX_ErrorNone;

  /** Temporary First Output buffer size */
  omx_shvpudec_component_Private->inputCurrBuffer = NULL;
  omx_shvpudec_component_Private->inputCurrLength = 0;
  omx_shvpudec_component_Private->isFirstBuffer = 1;
  omx_shvpudec_component_Private->isNewBuffer = 1;

  return eError;
}

/** The Deinitialization function of the video decoder  
  */
OMX_ERRORTYPE omx_shvpudec_component_Deinit(OMX_COMPONENTTYPE *openmaxStandComp) {

  omx_shvpudec_component_PrivateType* omx_shvpudec_component_Private = openmaxStandComp->pComponentPrivate;
  OMX_ERRORTYPE eError = OMX_ErrorNone;

  if (omx_shvpudec_component_Private->avcodecReady) {
    omx_shvpudec_component_ffmpegLibDeInit(omx_shvpudec_component_Private);
    omx_shvpudec_component_Private->avcodecReady = OMX_FALSE;
  }

  return eError;
} 

/** Executes all the required steps after an output buffer frame-size has changed.
*/
static inline void UpdateFrameSize(OMX_COMPONENTTYPE *openmaxStandComp) {
  omx_shvpudec_component_PrivateType* omx_shvpudec_component_Private = openmaxStandComp->pComponentPrivate;
  omx_base_video_PortType *outPort = (omx_base_video_PortType *)omx_shvpudec_component_Private->ports[OMX_BASE_FILTER_OUTPUTPORT_INDEX];
  omx_base_video_PortType *inPort = (omx_base_video_PortType *)omx_shvpudec_component_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX];
  outPort->sPortParam.format.video.nFrameWidth = inPort->sPortParam.format.video.nFrameWidth;
  outPort->sPortParam.format.video.nFrameHeight = inPort->sPortParam.format.video.nFrameHeight;
  switch(outPort->sVideoParam.eColorFormat) {
    case OMX_COLOR_FormatYUV420Planar:
      if(outPort->sPortParam.format.video.nFrameWidth && outPort->sPortParam.format.video.nFrameHeight) {
        outPort->sPortParam.nBufferSize = outPort->sPortParam.format.video.nFrameWidth * outPort->sPortParam.format.video.nFrameHeight * 3/2;
      }
      break;
    default:
      if(outPort->sPortParam.format.video.nFrameWidth && outPort->sPortParam.format.video.nFrameHeight) {
        outPort->sPortParam.nBufferSize = outPort->sPortParam.format.video.nFrameWidth * outPort->sPortParam.format.video.nFrameHeight * 3;
      }
      break;
  }
}

/* local output callback, should be static */
static int
vpu4_decoded (SHCodecs_Decoder * decoder,
              unsigned char * y_buf, int y_size,
              unsigned char * c_buf, int c_size,
              void * user_data)
{
  OMX_BUFFERHEADERTYPE* pOutputBuffer = (OMX_BUFFERHEADERTYPE *)user_data;
  OMX_U8* outputCurrBuffer;

  outputCurrBuffer = pOutputBuffer->pBuffer;

  memcpy (outputCurrBuffer, y_buf, y_size);
  outputCurrBuffer += y_size;
  pOutputBuffer->nFilledLen += y_size;

  memcpy (outputCurrBuffer, c_buf, c_size);
  pOutputBuffer->nFilledLen += c_size;

#if 0
    mp_image_t *mpi = (mp_image_t *)user_data;
    static unsigned char *c;
    int i;

    memcpy (mpi->planes[0], y_buf, y_size);

#if 0
    c = &mpi->planes[0][10000];
    printf ("\t%x%x%x%x %x%x%x%x\n", c[0], c[1], c[2], c[3], c[4], c[5], c[6], c[7]);
#endif

    c = c_buf;
    for (i = 0; i < c_size/2; i++) {
      mpi->planes[1][i] = *c++;
      mpi->planes[2][i] = *c++;
    }
#endif

    return 0;
}

/** This function is used to process the input buffer and provide one output buffer
  */
void omx_shvpudec_component_BufferMgmtCallback(OMX_COMPONENTTYPE *openmaxStandComp, OMX_BUFFERHEADERTYPE* pInputBuffer, OMX_BUFFERHEADERTYPE* pOutputBuffer) {

  omx_shvpudec_component_PrivateType* omx_shvpudec_component_Private = openmaxStandComp->pComponentPrivate;
  //AVPicture pic;


  OMX_S32 nOutputFilled = 0;
  OMX_U8* outputCurrBuffer;
  int ret = 0;
  int nLen = 0;
  int internalOutputFilled=0;
  int nSize;
  struct SwsContext *imgConvertYuvCtx = NULL;

  DEBUG(DEB_LEV_FUNCTION_NAME, "In %s\n", __func__);
  /** Fill up the current input buffer when a new buffer has arrived */
  if(omx_shvpudec_component_Private->isNewBuffer) {
    omx_shvpudec_component_Private->inputCurrBuffer = pInputBuffer->pBuffer;
    omx_shvpudec_component_Private->inputCurrLength = pInputBuffer->nFilledLen;
    omx_shvpudec_component_Private->isNewBuffer = 0;
    DEBUG(DEB_LEV_FULL_SEQ, "New Buffer FilledLen = %d\n", (int)pInputBuffer->nFilledLen);
  }

  outputCurrBuffer = pOutputBuffer->pBuffer;
  pOutputBuffer->nFilledLen = 0;
  pOutputBuffer->nOffset = 0;

  while (!nOutputFilled) {
    if (omx_shvpudec_component_Private->isFirstBuffer) {
      tsem_down(omx_shvpudec_component_Private->avCodecSyncSem);
      omx_shvpudec_component_Private->isFirstBuffer = 0;
    }

#if 0
    omx_shvpudec_component_Private->avCodecContext->frame_number++;

    nLen = avcodec_decode_video(omx_shvpudec_component_Private->avCodecContext, 
          omx_shvpudec_component_Private->avFrame, 
          (int*)&internalOutputFilled,
          omx_shvpudec_component_Private->inputCurrBuffer, 
          omx_shvpudec_component_Private->inputCurrLength);
#endif

    shcodecs_decoder_set_decoded_callback (omx_shvpudec_component_Private->decoder,
                                           vpu4_decoded, pOutputBuffer);

    ret = shcodecs_decode (omx_shvpudec_component_Private->decoder,
                           omx_shvpudec_component_Private->inputCurrBuffer, 
                           omx_shvpudec_component_Private->inputCurrLength);

    if (ret < 0) {
      DEBUG(DEB_LEV_ERR, "----> A general error or simply frame not decoded?\n");
    }

#if 0
    {
      omx_base_video_PortType *inPort = (omx_base_video_PortType *)omx_shvpudec_component_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX];
      if((inPort->sPortParam.format.video.nFrameWidth != omx_shvpudec_component_Private->avCodecContext->width) ||
         (inPort->sPortParam.format.video.nFrameHeight != omx_shvpudec_component_Private->avCodecContext->height)) {
        DEBUG(DEB_LEV_SIMPLE_SEQ, "---->Sending Port Settings Change Event in video decoder\n");

        switch(omx_shvpudec_component_Private->video_coding_type) {
          case OMX_VIDEO_CodingMPEG4 :
          case OMX_VIDEO_CodingAVC :
            inPort->sPortParam.format.video.nFrameWidth = omx_shvpudec_component_Private->avCodecContext->width;
            inPort->sPortParam.format.video.nFrameHeight = omx_shvpudec_component_Private->avCodecContext->height;
            break;
          default :
            DEBUG(DEB_LEV_ERR, "Video formats other than MPEG-4 AVC not supported\nCodec not found\n");
            break;           
        }

        UpdateFrameSize (openmaxStandComp);

        /** Send Port Settings changed call back */
        (*(omx_shvpudec_component_Private->callbacks->EventHandler))
          (openmaxStandComp,
           omx_shvpudec_component_Private->callbackData,
           OMX_EventPortSettingsChanged, // The command was completed 
           nLen,  //to adjust the file pointer to resume the correct decode process
           0, // This is the input port index 
           NULL);
      }
    }
#endif

    if ( ret >= 0 && internalOutputFilled) {
      omx_shvpudec_component_Private->inputCurrBuffer += nLen;
      omx_shvpudec_component_Private->inputCurrLength -= nLen;
      pInputBuffer->nFilledLen -= nLen;

      //Buffer is fully consumed. Request for new Input Buffer
      if(pInputBuffer->nFilledLen == 0) {
        omx_shvpudec_component_Private->isNewBuffer = 1;
      }
      
#if 0
      nSize = avpicture_get_size (omx_shvpudec_component_Private->eOutFramePixFmt,
                                  omx_shvpudec_component_Private->avCodecContext->width,
                                  omx_shvpudec_component_Private->avCodecContext->height);

      if(pOutputBuffer->nAllocLen < nSize) {
        DEBUG(DEB_LEV_ERR, "Ouch!!!! Output buffer Alloc Len %d less than Frame Size %d\n",(int)pOutputBuffer->nAllocLen,nSize);
        return;
      }
#endif

#if 0
      avpicture_fill (&pic, (unsigned char*)(outputCurrBuffer),
                      omx_shvpudec_component_Private->eOutFramePixFmt, 
                      omx_shvpudec_component_Private->avCodecContext->width, 
                      omx_shvpudec_component_Private->avCodecContext->height);

      if ( !imgConvertYuvCtx ) {
        imgConvertYuvCtx = sws_getContext( omx_shvpudec_component_Private->avCodecContext->width, 
                                              omx_shvpudec_component_Private->avCodecContext->height, 
                                              omx_shvpudec_component_Private->avCodecContext->pix_fmt,
                                              omx_shvpudec_component_Private->avCodecContext->width,
                                              omx_shvpudec_component_Private->avCodecContext->height,
                                              omx_shvpudec_component_Private->eOutFramePixFmt, SWS_FAST_BILINEAR, NULL, NULL, NULL );
      }

      sws_scale(imgConvertYuvCtx, omx_shvpudec_component_Private->avFrame->data, 
                omx_shvpudec_component_Private->avFrame->linesize, 0, 
                omx_shvpudec_component_Private->avCodecContext->height, pic.data, pic.linesize );

      if (imgConvertYuvCtx ) {
        sws_freeContext(imgConvertYuvCtx);
      }

      DEBUG(DEB_LEV_FULL_SEQ, "nSize=%d,frame linesize=%d,height=%d,pic linesize=%d PixFmt=%d\n",nSize,
        omx_shvpudec_component_Private->avFrame->linesize[0], 
        omx_shvpudec_component_Private->avCodecContext->height, 
        pic.linesize[0],omx_shvpudec_component_Private->eOutFramePixFmt);
#endif

      pOutputBuffer->nFilledLen += nSize;

    } else {
      /**  This condition becomes true when the input buffer has completely be consumed.
        * In this case is immediately switched because there is no real buffer consumption 
        */
      pInputBuffer->nFilledLen = 0;
      /** Few bytes may be left in the input buffer but can't generate one output frame. 
        *  Request for new Input Buffer
        */
      omx_shvpudec_component_Private->isNewBuffer = 1;
      pOutputBuffer->nFilledLen = 0;
    }

    nOutputFilled = 1;
  }
  DEBUG(DEB_LEV_FULL_SEQ, "One output buffer %x nLen=%d is full returning in video decoder\n", 
            (int)pOutputBuffer->pBuffer, (int)pOutputBuffer->nFilledLen);
}

OMX_ERRORTYPE omx_shvpudec_component_SetParameter(
OMX_IN  OMX_HANDLETYPE hComponent,
OMX_IN  OMX_INDEXTYPE nParamIndex,
OMX_IN  OMX_PTR ComponentParameterStructure) {

  OMX_ERRORTYPE eError = OMX_ErrorNone;
  OMX_U32 portIndex;

  /* Check which structure we are being fed and make control its header */
  OMX_COMPONENTTYPE *openmaxStandComp = hComponent;
  omx_shvpudec_component_PrivateType* omx_shvpudec_component_Private = openmaxStandComp->pComponentPrivate;
  omx_base_video_PortType *port;
  if (ComponentParameterStructure == NULL) {
    return OMX_ErrorBadParameter;
  }

  DEBUG(DEB_LEV_SIMPLE_SEQ, "   Setting parameter %i\n", nParamIndex);
  switch(nParamIndex) {
    case OMX_IndexParamPortDefinition:
      {
        eError = omx_base_component_SetParameter(hComponent, nParamIndex, ComponentParameterStructure);
        if(eError == OMX_ErrorNone) {
          OMX_PARAM_PORTDEFINITIONTYPE *pPortDef = (OMX_PARAM_PORTDEFINITIONTYPE*)ComponentParameterStructure;
          UpdateFrameSize (openmaxStandComp);
          portIndex = pPortDef->nPortIndex;
          port = (omx_base_video_PortType *)omx_shvpudec_component_Private->ports[portIndex];
          port->sVideoParam.eColorFormat = port->sPortParam.format.video.eColorFormat;
        }
        break;
      }
    case OMX_IndexParamVideoPortFormat:
      {
        OMX_VIDEO_PARAM_PORTFORMATTYPE *pVideoPortFormat;
        pVideoPortFormat = ComponentParameterStructure;
        portIndex = pVideoPortFormat->nPortIndex;
        /*Check Structure Header and verify component state*/
        eError = omx_base_component_ParameterSanityCheck(hComponent, portIndex, pVideoPortFormat, sizeof(OMX_VIDEO_PARAM_PORTFORMATTYPE));
        if(eError!=OMX_ErrorNone) { 
          DEBUG(DEB_LEV_ERR, "In %s Parameter Check Error=%x\n",__func__,eError); 
          break;
        } 
        if (portIndex <= 1) {
          port = (omx_base_video_PortType *)omx_shvpudec_component_Private->ports[portIndex];
          memcpy(&port->sVideoParam, pVideoPortFormat, sizeof(OMX_VIDEO_PARAM_PORTFORMATTYPE));
          omx_shvpudec_component_Private->ports[portIndex]->sPortParam.format.video.eColorFormat = port->sVideoParam.eColorFormat;

          if (portIndex == 1) {
#if 0
            switch(port->sVideoParam.eColorFormat) {
              case OMX_COLOR_Format24bitRGB888 :
                omx_shvpudec_component_Private->eOutFramePixFmt = PIX_FMT_RGB24;
                break; 
              case OMX_COLOR_Format24bitBGR888 :
                omx_shvpudec_component_Private->eOutFramePixFmt = PIX_FMT_BGR24;
                break;
              case OMX_COLOR_Format32bitBGRA8888 :
                omx_shvpudec_component_Private->eOutFramePixFmt = PIX_FMT_BGR32;
                break;
              case OMX_COLOR_Format32bitARGB8888 :
                omx_shvpudec_component_Private->eOutFramePixFmt = PIX_FMT_RGB32;
                break; 
              case OMX_COLOR_Format16bitARGB1555 :
                omx_shvpudec_component_Private->eOutFramePixFmt = PIX_FMT_RGB555;
                break;
              case OMX_COLOR_Format16bitRGB565 :
                omx_shvpudec_component_Private->eOutFramePixFmt = PIX_FMT_RGB565;
                break; 
              case OMX_COLOR_Format16bitBGR565 :
                omx_shvpudec_component_Private->eOutFramePixFmt = PIX_FMT_BGR565;
                break;
              default:
                omx_shvpudec_component_Private->eOutFramePixFmt = PIX_FMT_YUV420P;
                break;
            }
#endif
            UpdateFrameSize (openmaxStandComp);
          }
        } else {
          return OMX_ErrorBadPortIndex;
        }
        break;
      }
    case OMX_IndexParamVideoAvc:
      {
        OMX_VIDEO_PARAM_AVCTYPE *pVideoAvc;
        pVideoAvc = ComponentParameterStructure;
        portIndex = pVideoAvc->nPortIndex;
        eError = omx_base_component_ParameterSanityCheck(hComponent, portIndex, pVideoAvc, sizeof(OMX_VIDEO_PARAM_AVCTYPE));
        if(eError!=OMX_ErrorNone) { 
          DEBUG(DEB_LEV_ERR, "In %s Parameter Check Error=%x\n",__func__,eError); 
          break;
        } 
        memcpy(&omx_shvpudec_component_Private->pVideoAvc, pVideoAvc, sizeof(OMX_VIDEO_PARAM_AVCTYPE));
        break;
      }
    case OMX_IndexParamStandardComponentRole:
      {
        OMX_PARAM_COMPONENTROLETYPE *pComponentRole;
        pComponentRole = ComponentParameterStructure;
        if (omx_shvpudec_component_Private->state != OMX_StateLoaded && omx_shvpudec_component_Private->state != OMX_StateWaitForResources) {
          DEBUG(DEB_LEV_ERR, "In %s Incorrect State=%x lineno=%d\n",__func__,omx_shvpudec_component_Private->state,__LINE__);
          return OMX_ErrorIncorrectStateOperation;
        }
  
        if ((eError = checkHeader(ComponentParameterStructure, sizeof(OMX_PARAM_COMPONENTROLETYPE))) != OMX_ErrorNone) { 
          break;
        }

        if (!strcmp((char *)pComponentRole->cRole, VIDEO_DEC_MPEG4_ROLE)) {
          omx_shvpudec_component_Private->video_coding_type = OMX_VIDEO_CodingMPEG4;
        } else if (!strcmp((char *)pComponentRole->cRole, VIDEO_DEC_H264_ROLE)) {
          omx_shvpudec_component_Private->video_coding_type = OMX_VIDEO_CodingAVC;
        } else {
          return OMX_ErrorBadParameter;
        }
        SetInternalVideoParameters(openmaxStandComp);
        break;
      }
    case OMX_IndexParamVideoMpeg4:
      {
        OMX_VIDEO_PARAM_MPEG4TYPE *pVideoMpeg4;
        pVideoMpeg4 = ComponentParameterStructure;
        portIndex = pVideoMpeg4->nPortIndex;
        eError = omx_base_component_ParameterSanityCheck(hComponent, portIndex, pVideoMpeg4, sizeof(OMX_VIDEO_PARAM_MPEG4TYPE));
        if(eError!=OMX_ErrorNone) { 
          DEBUG(DEB_LEV_ERR, "In %s Parameter Check Error=%x\n",__func__,eError); 
          break;
        } 
        if (pVideoMpeg4->nPortIndex == 0) {
          memcpy(&omx_shvpudec_component_Private->pVideoMpeg4, pVideoMpeg4, sizeof(OMX_VIDEO_PARAM_MPEG4TYPE));
        } else {
          return OMX_ErrorBadPortIndex;
        }
        break;
      }
    default: /*Call the base component function*/
      return omx_base_component_SetParameter(hComponent, nParamIndex, ComponentParameterStructure);
  }
  return eError;
}

OMX_ERRORTYPE omx_shvpudec_component_GetParameter(
  OMX_IN  OMX_HANDLETYPE hComponent,
  OMX_IN  OMX_INDEXTYPE nParamIndex,
  OMX_INOUT OMX_PTR ComponentParameterStructure) {

  omx_base_video_PortType *port;
  OMX_ERRORTYPE eError = OMX_ErrorNone;

  OMX_COMPONENTTYPE *openmaxStandComp = hComponent;
  omx_shvpudec_component_PrivateType* omx_shvpudec_component_Private = openmaxStandComp->pComponentPrivate;
  if (ComponentParameterStructure == NULL) {
    return OMX_ErrorBadParameter;
  }
  DEBUG(DEB_LEV_SIMPLE_SEQ, "   Getting parameter %i\n", nParamIndex);
  /* Check which structure we are being fed and fill its header */
  switch(nParamIndex) {
    case OMX_IndexParamVideoInit:
      if ((eError = checkHeader(ComponentParameterStructure, sizeof(OMX_PORT_PARAM_TYPE))) != OMX_ErrorNone) { 
        break;
      }
      memcpy(ComponentParameterStructure, &omx_shvpudec_component_Private->sPortTypesParam[OMX_PortDomainVideo], sizeof(OMX_PORT_PARAM_TYPE));
      break;    
    case OMX_IndexParamVideoPortFormat:
      {
        OMX_VIDEO_PARAM_PORTFORMATTYPE *pVideoPortFormat;  
        pVideoPortFormat = ComponentParameterStructure;
        if ((eError = checkHeader(ComponentParameterStructure, sizeof(OMX_VIDEO_PARAM_PORTFORMATTYPE))) != OMX_ErrorNone) { 
          break;
        }
        if (pVideoPortFormat->nPortIndex <= 1) {
          port = (omx_base_video_PortType *)omx_shvpudec_component_Private->ports[pVideoPortFormat->nPortIndex];
          memcpy(pVideoPortFormat, &port->sVideoParam, sizeof(OMX_VIDEO_PARAM_PORTFORMATTYPE));
        } else {
          return OMX_ErrorBadPortIndex;
        }
        break;    
      }
    case OMX_IndexParamVideoMpeg4:
      {
        OMX_VIDEO_PARAM_MPEG4TYPE *pVideoMpeg4;
        pVideoMpeg4 = ComponentParameterStructure;
        if (pVideoMpeg4->nPortIndex != 0) {
          return OMX_ErrorBadPortIndex;
        }
        if ((eError = checkHeader(ComponentParameterStructure, sizeof(OMX_VIDEO_PARAM_MPEG4TYPE))) != OMX_ErrorNone) { 
          break;
        }
        memcpy(pVideoMpeg4, &omx_shvpudec_component_Private->pVideoMpeg4, sizeof(OMX_VIDEO_PARAM_MPEG4TYPE));
        break;
      }
    case OMX_IndexParamVideoAvc:
      {
        OMX_VIDEO_PARAM_AVCTYPE * pVideoAvc; 
        pVideoAvc = ComponentParameterStructure;
        if (pVideoAvc->nPortIndex != 0) {
          return OMX_ErrorBadPortIndex;
        }
        if ((eError = checkHeader(ComponentParameterStructure, sizeof(OMX_VIDEO_PARAM_AVCTYPE))) != OMX_ErrorNone) { 
          break;
        }
        memcpy(pVideoAvc, &omx_shvpudec_component_Private->pVideoAvc, sizeof(OMX_VIDEO_PARAM_AVCTYPE));
        break;
      }
    case OMX_IndexParamStandardComponentRole:
      {
        OMX_PARAM_COMPONENTROLETYPE * pComponentRole;
        pComponentRole = ComponentParameterStructure;
        if ((eError = checkHeader(ComponentParameterStructure, sizeof(OMX_PARAM_COMPONENTROLETYPE))) != OMX_ErrorNone) { 
          break;
        }
        if (omx_shvpudec_component_Private->video_coding_type == OMX_VIDEO_CodingMPEG4) {
          strcpy((char *)pComponentRole->cRole, VIDEO_DEC_MPEG4_ROLE);
        } else if (omx_shvpudec_component_Private->video_coding_type == OMX_VIDEO_CodingAVC) {
          strcpy((char *)pComponentRole->cRole, VIDEO_DEC_H264_ROLE);
        } else {
          strcpy((char *)pComponentRole->cRole,"\0");
        }
        break;
      }
    default: /*Call the base component function*/
      return omx_base_component_GetParameter(hComponent, nParamIndex, ComponentParameterStructure);
  }
  return OMX_ErrorNone;
}

OMX_ERRORTYPE omx_shvpudec_component_MessageHandler(OMX_COMPONENTTYPE* openmaxStandComp,internalRequestMessageType *message) {
  omx_shvpudec_component_PrivateType* omx_shvpudec_component_Private = (omx_shvpudec_component_PrivateType*)openmaxStandComp->pComponentPrivate;
  OMX_ERRORTYPE err;
  OMX_STATETYPE eCurrentState = omx_shvpudec_component_Private->state;

  DEBUG(DEB_LEV_FUNCTION_NAME, "In %s\n", __func__);

  if (message->messageType == OMX_CommandStateSet){
    if ((message->messageParam == OMX_StateExecuting ) && (omx_shvpudec_component_Private->state == OMX_StateIdle)) {
      if (!omx_shvpudec_component_Private->avcodecReady) {
        err = omx_shvpudec_component_ffmpegLibInit(omx_shvpudec_component_Private);
        if (err != OMX_ErrorNone) {
          return OMX_ErrorNotReady;
        }
        omx_shvpudec_component_Private->avcodecReady = OMX_TRUE;
      }
    } 
    else if ((message->messageParam == OMX_StateIdle ) && (omx_shvpudec_component_Private->state == OMX_StateLoaded)) {
      err = omx_shvpudec_component_Init(openmaxStandComp);
      if(err!=OMX_ErrorNone) { 
        DEBUG(DEB_LEV_ERR, "In %s Video Decoder Init Failed Error=%x\n",__func__,err); 
        return err;
      } 
    } else if ((message->messageParam == OMX_StateLoaded) && (omx_shvpudec_component_Private->state == OMX_StateIdle)) {
      err = omx_shvpudec_component_Deinit(openmaxStandComp);
      if(err!=OMX_ErrorNone) { 
        DEBUG(DEB_LEV_ERR, "In %s Video Decoder Deinit Failed Error=%x\n",__func__,err); 
        return err;
      } 
    }
  }
  // Execute the base message handling
  err =  omx_base_component_MessageHandler(openmaxStandComp,message);

  if (message->messageType == OMX_CommandStateSet){
   if ((message->messageParam == OMX_StateIdle  ) && (eCurrentState == OMX_StateExecuting)) {
      if (omx_shvpudec_component_Private->avcodecReady) {
        omx_shvpudec_component_ffmpegLibDeInit(omx_shvpudec_component_Private);
        omx_shvpudec_component_Private->avcodecReady = OMX_FALSE;
      }
    }
  }
  return err;
}
OMX_ERRORTYPE omx_shvpudec_component_ComponentRoleEnum(
  OMX_IN OMX_HANDLETYPE hComponent,
  OMX_OUT OMX_U8 *cRole,
  OMX_IN OMX_U32 nIndex) {

  if (nIndex == 0) {
    strcpy((char *)cRole, VIDEO_DEC_MPEG4_ROLE);
  } else if (nIndex == 1) {
    strcpy((char *)cRole, VIDEO_DEC_H264_ROLE);
  }  else {
    return OMX_ErrorUnsupportedIndex;
  }
  return OMX_ErrorNone;
}

OMX_ERRORTYPE omx_shvpudec_component_SetConfig(
  OMX_HANDLETYPE hComponent,
  OMX_INDEXTYPE nIndex,
  OMX_PTR pComponentConfigStructure) {

  OMX_ERRORTYPE err = OMX_ErrorNone;
  OMX_VENDOR_EXTRADATATYPE* pExtradata;

  OMX_COMPONENTTYPE *openmaxStandComp = (OMX_COMPONENTTYPE *)hComponent;
  omx_shvpudec_component_PrivateType* omx_shvpudec_component_Private = (omx_shvpudec_component_PrivateType*)openmaxStandComp->pComponentPrivate;
  if (pComponentConfigStructure == NULL) {
    return OMX_ErrorBadParameter;
  }
  DEBUG(DEB_LEV_SIMPLE_SEQ, "   Getting configuration %i\n", nIndex);
  /* Check which structure we are being fed and fill its header */
  switch (nIndex) {
    case OMX_IndexVendorVideoExtraData :
      pExtradata = (OMX_VENDOR_EXTRADATATYPE*)pComponentConfigStructure;
      if (pExtradata->nPortIndex <= 1) {
        /** copy the extradata in the codec context private structure */
#if 0
        omx_shvpudec_component_Private->extradata_size = (OMX_U32)pExtradata->nDataSize;
        if(omx_shvpudec_component_Private->extradata_size > 0) {
          if(omx_shvpudec_component_Private->extradata) {
            free(omx_shvpudec_component_Private->extradata);
          }
          omx_shvpudec_component_Private->extradata = malloc((int)pExtradata->nDataSize*sizeof(char));
          memcpy(omx_shvpudec_component_Private->extradata, (unsigned char *)pExtradata->pData,pExtradata->nDataSize);
        } else {
                  DEBUG(DEB_LEV_SIMPLE_SEQ,"extradata size is 0 !!!\n");
        }
#endif
      } else {
          return OMX_ErrorBadPortIndex;
      }
      break;

    default: // delegate to superclass
      return omx_base_component_SetConfig(hComponent, nIndex, pComponentConfigStructure);
  }
  return err;
}

OMX_ERRORTYPE omx_shvpudec_component_GetExtensionIndex(
  OMX_IN  OMX_HANDLETYPE hComponent,
  OMX_IN  OMX_STRING cParameterName,
  OMX_OUT OMX_INDEXTYPE* pIndexType) {

  DEBUG(DEB_LEV_FUNCTION_NAME,"In  %s \n",__func__);

  if(strcmp(cParameterName,"OMX.ST.index.config.videoextradata") == 0) {
    *pIndexType = OMX_IndexVendorVideoExtraData;
  } else {
    return OMX_ErrorBadParameter;
  }
  return OMX_ErrorNone;
}
