// myclient.cpp : 定义控制台应用程序的入口点。
//

#include "stdafx.h"

#include <stdlib.h>
#include <apr_getopt.h>
#include <apr_file_info.h>
#include "apt_pool.h"
#include "apt_log.h"
#include "apt_dir_layout.h"
#include "unimrcp_client.h"
#include "mrcp_application.h"
#include "mrcp_synth_header.h"
#include "mrcp_synth_resource.h"
#include "mrcp_recog_header.h"
#include "mrcp_recog_resource.h"
#include "mrcp_message.h"
#include "mrcp_generic_header.h"
#include "mpf_termination_factory.h"
#include "mpf_termination.h"




/** Declaration of synthesizer audio stream methods */
static apt_bool_t synth_app_stream_destroy(mpf_audio_stream_t *stream);
static apt_bool_t synth_app_stream_open(mpf_audio_stream_t *stream, mpf_codec_t *codec);
static apt_bool_t synth_app_stream_close(mpf_audio_stream_t *stream);
static apt_bool_t synth_app_stream_write(mpf_audio_stream_t *stream, const mpf_frame_t *frame);
static apt_bool_t recog_app_stream_open(mpf_audio_stream_t *stream, mpf_codec_t *codec);
static apt_bool_t recog_app_stream_close(mpf_audio_stream_t *stream);
static apt_bool_t recog_app_stream_read(mpf_audio_stream_t *stream, mpf_frame_t *frame);

static const mpf_audio_stream_vtable_t audio_stream_vtable = {
	synth_app_stream_destroy,
	recog_app_stream_open,
	recog_app_stream_close,
	recog_app_stream_read,
	synth_app_stream_open,
	synth_app_stream_close,
	synth_app_stream_write,
	NULL
};


static apt_bool_t synth_application_on_message_receive(mrcp_application_t *application, mrcp_session_t *session, mrcp_channel_t *channel, mrcp_message_t *message);
static apt_bool_t synth_application_on_channel_remove(mrcp_application_t *application, mrcp_session_t *session, mrcp_channel_t *channel, mrcp_sig_status_code_e status);
static apt_bool_t synth_application_on_session_terminate(mrcp_application_t *application, mrcp_session_t *session, mrcp_sig_status_code_e status);
static apt_bool_t recog_application_on_message_receive(mrcp_application_t *application, mrcp_session_t *session, mrcp_channel_t *channel, mrcp_message_t *message);
static apt_bool_t recog_application_on_channel_remove(mrcp_application_t *application, mrcp_session_t *session, mrcp_channel_t *channel, mrcp_sig_status_code_e status);
static apt_bool_t recog_application_on_session_terminate(mrcp_application_t *application, mrcp_session_t *session, mrcp_sig_status_code_e status);

static const mrcp_app_message_dispatcher_t synth_application_dispatcher = {
	NULL /*synth_application_on_session_update*/,
	synth_application_on_session_terminate,
	NULL /*synth_application_on_channel_add*/,
	synth_application_on_channel_remove,
	synth_application_on_message_receive,
	NULL /* synth_application_on_terminate_event */,
	NULL /* synth_application_on_resource_discover */
};

static const mrcp_app_message_dispatcher_t recog_application_dispatcher = {
	NULL /*synth_application_on_session_update*/,
	recog_application_on_session_terminate,
	NULL /*synth_application_on_channel_add*/,
	recog_application_on_channel_remove,
	recog_application_on_message_receive,
	NULL /* synth_application_on_terminate_event */,
	NULL /* synth_application_on_resource_discover */
};


static apt_bool_t synth_application_on_channel_remove(mrcp_application_t *application, mrcp_session_t *session, mrcp_channel_t *channel, mrcp_sig_status_code_e status)
{
	mrcp_application_session_terminate(session);
	return TRUE;
}

static apt_bool_t synth_application_on_session_terminate(mrcp_application_t *application, mrcp_session_t *session, mrcp_sig_status_code_e status)
{
	mrcp_application_session_destroy(session);
	return TRUE;
}

static apt_bool_t synth_application_on_message_receive(mrcp_application_t *application, mrcp_session_t *session, mrcp_channel_t *channel, mrcp_message_t *message)
{
		if(message->start_line.message_type == MRCP_MESSAGE_TYPE_RESPONSE) {
		/* received MRCP response */
		if(message->start_line.method_id == SYNTHESIZER_SPEAK) {
			/* received the response to SPEAK request */
			if(message->start_line.request_state == MRCP_REQUEST_STATE_INPROGRESS) {
				/* waiting for SPEAK-COMPLETE event */
				OutputDebugString("!!MRCP_REQUEST_STATE_INPROGRESS\n");
			}
			else {
				/* received unexpected response, remove channel */
				mrcp_application_channel_remove(session,channel);
			}
		}
		else {
			/* received unexpected response */
		}
	}
	else if(message->start_line.message_type == MRCP_MESSAGE_TYPE_EVENT) {
		/* received MRCP event */
		if(message->start_line.method_id == SYNTHESIZER_SPEAK_COMPLETE) {
			/* received SPEAK-COMPLETE event, remove channel */
			OutputDebugString("!!SYNTHESIZER_SPEAK_COMPLETE\n");
			mrcp_application_channel_remove(session,channel);
		}
	}
	return TRUE;
}

static apt_bool_t recog_application_on_channel_remove(mrcp_application_t *application, mrcp_session_t *session, mrcp_channel_t *channel, mrcp_sig_status_code_e status)
{
	mrcp_application_session_terminate(session);
	return TRUE;
}

static apt_bool_t recog_application_on_session_terminate(mrcp_application_t *application, mrcp_session_t *session, mrcp_sig_status_code_e status)
{
	mrcp_application_session_destroy(session);
	return TRUE;
}

static apt_bool_t recog_application_on_message_receive(mrcp_application_t *application, mrcp_session_t *session, mrcp_channel_t *channel, mrcp_message_t *message)
{
		if(message->start_line.message_type == MRCP_MESSAGE_TYPE_RESPONSE) {
		/* received MRCP response */
		if(message->start_line.method_id == RECOGNIZER_DEFINE_GRAMMAR) {
			/* received the response to DEFINE-GRAMMAR request */
			if(message->start_line.request_state == MRCP_REQUEST_STATE_COMPLETE) {
				//recog_application_on_define_grammar(application,session,channel);
				OutputDebugString("!!RECOGNIZER_DEFINE_GRAMMAR\n");
			}
			else {
				/* received unexpected response, remove channel */
				mrcp_application_channel_remove(session,channel);
			}
		}
		else if(message->start_line.method_id == RECOGNIZER_RECOGNIZE) {
			/* received the response to SPEAK request */
			if(message->start_line.request_state == MRCP_REQUEST_STATE_INPROGRESS) {
				/* waiting for SPEAK-COMPLETE event */
				OutputDebugString("!!MRCP_REQUEST_STATE_INPROGRESS\n");
			}
			else {
				/* received unexpected response, remove channel */
				mrcp_application_channel_remove(session,channel);
			}
		}
		else {
			/* received unexpected response */
		}
	}
	else if(message->start_line.message_type == MRCP_MESSAGE_TYPE_EVENT) {
		/* received MRCP event */
		if(message->start_line.method_id == RECOGNIZER_RECOGNITION_COMPLETE) {
			/* received SPEAK-COMPLETE event, remove channel */
			OutputDebugString("!!RECOGNIZER_RECOGNITION_COMPLETE\n");
			OutputDebugString(message->body.buf);
			mrcp_application_channel_remove(session,channel);
		}

		if(message->start_line.method_id == RECOGNIZER_START_OF_INPUT) {
			/* received SPEAK-COMPLETE event, remove channel */
			OutputDebugString("!!RECOGNIZER_START_OF_INPUT\n");
		//	mrcp_application_channel_remove(session,channel);
		}
	}
	return TRUE;
}

/** Callback is called from MPF engine context to destroy any additional data associated with audio stream */
static apt_bool_t synth_app_stream_destroy(mpf_audio_stream_t *stream)
{
	/* nothing to destroy in demo */
	OutputDebugString("!!synth_app_stream_destroy\n");
	return TRUE;
}

/** Callback is called from MPF engine context to perform application stream specific action before open */
static apt_bool_t synth_app_stream_open(mpf_audio_stream_t *stream, mpf_codec_t *codec)
{
	OutputDebugString("!!synth_app_stream_open\n");
	return TRUE;
}

/** Callback is called from MPF engine context to perform application stream specific action after close */
static apt_bool_t synth_app_stream_close(mpf_audio_stream_t *stream)
{
	fclose((FILE*)stream->obj);
	OutputDebugString("!!synth_app_stream_close\n");
	return TRUE;
}

/** Callback is called from MPF engine context to make new frame available to write/send */
static apt_bool_t synth_app_stream_write(mpf_audio_stream_t *stream, const mpf_frame_t *frame)
{
	fwrite(frame->codec_frame.buffer,1,frame->codec_frame.size,(FILE*)stream->obj);
	OutputDebugString("!!synth_app_stream_write\n");
	return TRUE;
}

/** Callback is called from MPF engine context to perform application stream specific action before open */
static apt_bool_t recog_app_stream_open(mpf_audio_stream_t *stream, mpf_codec_t *codec)
{
	OutputDebugString("!!recog_app_stream_open\n");
	return TRUE;
}

/** Callback is called from MPF engine context to perform application stream specific action after close */
static apt_bool_t recog_app_stream_close(mpf_audio_stream_t *stream)
{
	fclose((FILE*)stream->obj);
	OutputDebugString("!!recog_app_stream_close\n");
	return TRUE;
}

/** Callback is called from MPF engine context to make new frame available to write/send */
static apt_bool_t recog_app_stream_read(mpf_audio_stream_t *stream,mpf_frame_t *frame)
{
	//fwrite(frame->codec_frame.buffer,1,frame->codec_frame.size,(FILE*)stream->obj);
	//fread(frame->codec_frame.buffer,1,frame->codec_frame.size,(FILE*)stream->obj);

	if(fread(frame->codec_frame.buffer,1,frame->codec_frame.size,(FILE*)stream->obj) == frame->codec_frame.size) {
				/* normal read */
		frame->type |= MEDIA_FRAME_TYPE_AUDIO;
	}
	//OutputDebugString("!!recog_app_stream_read\n");
	return TRUE;
}


static apt_bool_t my_synth_message_handler(const mrcp_app_message_t *app_message)
{
	return mrcp_application_message_dispatch(&synth_application_dispatcher,app_message);
}

static apt_bool_t my_recog_message_handler(const mrcp_app_message_t *app_message)
{
	return mrcp_application_message_dispatch(&recog_application_dispatcher,app_message);
}


mrcp_message_t* my_speak_message_create(mrcp_session_t *session, mrcp_channel_t *channel, const apt_dir_layout_t *dir_layout)
{
	/* create MRCP message */
	mrcp_message_t *mrcp_message = mrcp_application_message_create(session,channel,SYNTHESIZER_SPEAK);
	if(mrcp_message) {
		mrcp_generic_header_t *generic_header;
		mrcp_synth_header_t *synth_header;
		/* get/allocate generic header */
		generic_header = mrcp_generic_header_prepare(mrcp_message);
		if(generic_header) {
			/* set generic header fields */
			apt_string_assign(&generic_header->content_type,"text/plain",mrcp_message->pool);
			mrcp_generic_header_property_add(mrcp_message,GENERIC_HEADER_CONTENT_TYPE);
		}
		/* get/allocate synthesizer header */
		synth_header = (mrcp_synth_header_t *)mrcp_resource_header_prepare(mrcp_message);
		if(synth_header) {
			/* set synthesizer header fields */
			synth_header->voice_param.age = 28;
			mrcp_resource_header_property_add(mrcp_message,SYNTHESIZER_HEADER_VOICE_AGE);
		}
	apt_string_assign_n(&mrcp_message->body,"1234567890",10,mrcp_message->pool);
	}
	return mrcp_message;
}

mrcp_message_t* my_recog_message_create(mrcp_session_t *session, mrcp_channel_t *channel, const apt_dir_layout_t *dir_layout)
{
	const char text[] = "session:request1@form-level.store";

	/* create MRCP message */
	mrcp_message_t *mrcp_message = mrcp_application_message_create(session,channel,RECOGNIZER_RECOGNIZE);
	if(mrcp_message) {
		mrcp_recog_header_t *recog_header;
		mrcp_generic_header_t *generic_header;
		/* get/allocate generic header */
		generic_header = mrcp_generic_header_prepare(mrcp_message);
		if(generic_header) {
			/* set generic header fields */
			apt_string_assign(&generic_header->content_type,"text/uri-list",mrcp_message->pool);
			mrcp_generic_header_property_add(mrcp_message,GENERIC_HEADER_CONTENT_TYPE);
		}
		/* get/allocate recognizer header */
		recog_header =(mrcp_recog_header_t *) mrcp_resource_header_prepare(mrcp_message);
		if(recog_header) {
			if(mrcp_message->start_line.version == MRCP_VERSION_2) {
				/* set recognizer header fields */
				recog_header->cancel_if_queue = FALSE;
				mrcp_resource_header_property_add(mrcp_message,RECOGNIZER_HEADER_CANCEL_IF_QUEUE);
			}
			recog_header->no_input_timeout = 5000;
			mrcp_resource_header_property_add(mrcp_message,RECOGNIZER_HEADER_NO_INPUT_TIMEOUT);
			recog_header->recognition_timeout = 10000;
			mrcp_resource_header_property_add(mrcp_message,RECOGNIZER_HEADER_RECOGNITION_TIMEOUT);
			recog_header->start_input_timers = TRUE;
			mrcp_resource_header_property_add(mrcp_message,RECOGNIZER_HEADER_START_INPUT_TIMERS);
			recog_header->confidence_threshold = 0.87f;
			mrcp_resource_header_property_add(mrcp_message,RECOGNIZER_HEADER_CONFIDENCE_THRESHOLD);
		}
		/* set message body */
		apt_string_assign(&mrcp_message->body,text,mrcp_message->pool);
	}
	return mrcp_message;
}


int _tmain(int argc, _TCHAR* argv[])
{
	apr_pool_t *pool;
	const char *log_conf_path;
	apt_dir_layout_t *dir_layout = NULL;

	char tempstr[256];
	GetCurrentDirectory(256,tempstr);
	printf(tempstr);

	SetConsoleOutputCP (65001);
	CONSOLE_FONT_INFOEX info = { 0 }; // 以下设置字体来支持中文显示。    
	info.cbSize = sizeof(info);    
	info.dwFontSize.Y = 16; // leave X as zero    
	info.FontWeight = FW_NORMAL;    
	wcscpy(info.FaceName, L"Consolas");    
	SetCurrentConsoleFontEx(GetStdHandle(STD_OUTPUT_HANDLE), NULL, &info);


	const char* root_dir_path = "..\\..\\";

	/* APR global initialization */
	if(apr_initialize() != APR_SUCCESS) {
		apr_terminate();
		return 0;
	}

	/* create APR pool */
	pool = apt_pool_create();
	if(!pool) {
		apr_terminate();
		return 0;
	}


	dir_layout = apt_default_dir_layout_create(root_dir_path,pool);

	if(!dir_layout) {
		printf("Failed to Create Directories Layout\n");
		apr_pool_destroy(pool);
		apr_terminate();
		return 0;
	}

	/* get path to logger configuration file */
	log_conf_path = apt_confdir_filepath_get(dir_layout,"logger.xml",pool);
	/* create and load singleton logger */
	apt_log_instance_load(log_conf_path,pool);


	if(apt_log_output_mode_check(APT_LOG_OUTPUT_FILE) == TRUE) {
		/* open the log file */
		const char *log_dir_path = apt_dir_layout_path_get(dir_layout,APT_LAYOUT_LOG_DIR);
		apt_log_file_open(log_dir_path,"unimrcpclient",MAX_LOG_FILE_SIZE,MAX_LOG_FILE_COUNT,FALSE,pool);
	}

	mrcp_client_t *client = unimrcp_client_create(dir_layout);

//	mrcp_application_t* mrcp_synth_app = mrcp_application_create(my_synth_message_handler,NULL,pool);
//	mrcp_client_application_register(client,mrcp_synth_app,"synth");
	mrcp_application_t* mrcp_recog_app = mrcp_application_create(my_recog_message_handler,NULL,pool);
	mrcp_client_application_register(client,mrcp_recog_app,"recog");

	mrcp_client_start(client);

	
//	mrcp_session_t * mrcp_synth_session = mrcp_application_session_create(mrcp_synth_app,"uni2",NULL);

	mrcp_session_t * mrcp_recog_session = mrcp_application_session_create(mrcp_recog_app,"uni2",NULL);

	mpf_stream_capabilities_t *capabilities = mpf_source_stream_capabilities_create(pool);



	mpf_codec_capabilities_add(
			&capabilities->codecs,
			MPF_SAMPLE_RATE_8000,
			"PCMA");


	FILE* myfile1 = fopen("c:\\test.pcm","wb");

	//mpf_termination_t* termination = mpf_termination_create(termination_factory, NULL, pool);
//	mpf_termination_t* synth_termination =  mrcp_application_audio_termination_create(
//			mrcp_synth_session,                   /* session, termination belongs to */
//			&audio_stream_vtable,      /* virtual methods table of audio stream */
//			capabilities,              /* capabilities of audio stream */
//			myfile1);            /* object to associate */;

//	mrcp_channel_t * mrcp_synth_channel = mrcp_application_channel_create(mrcp_synth_session,MRCP_SYNTHESIZER_RESOURCE,synth_termination,NULL,NULL);

//	mrcp_application_channel_add(mrcp_synth_session,mrcp_synth_channel);


	FILE* myfile2 = fopen("c:\\recog.pcm","rb");

		//mpf_termination_t* termination = mpf_termination_create(termination_factory, NULL, pool);
	mpf_termination_t* recog_termination =  mrcp_application_audio_termination_create(
			mrcp_recog_session,                   /* session, termination belongs to */
			&audio_stream_vtable,      /* virtual methods table of audio stream */
			capabilities,              /* capabilities of audio stream */
			myfile2);            /* object to associate */;

	mrcp_channel_t * mrcp_recog_channel = mrcp_application_channel_create(mrcp_recog_session,MRCP_RECOGNIZER_RESOURCE,recog_termination,NULL,NULL);

	mrcp_application_channel_add(mrcp_recog_session,mrcp_recog_channel);

	//////////////////////////////////////////////////////////////////////
//	mrcp_message_t *mrcp_synth_message = my_speak_message_create(mrcp_synth_session,mrcp_synth_channel,dir_layout);

//	mrcp_application_message_send(mrcp_synth_session,mrcp_synth_channel,mrcp_synth_message);


	mrcp_message_t *mrcp_recog_message = my_recog_message_create(mrcp_recog_session,mrcp_recog_channel,dir_layout);

	mrcp_application_message_send(mrcp_recog_session,mrcp_recog_channel,mrcp_recog_message);

	getchar();

	return 0;
}

