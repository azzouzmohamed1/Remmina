/*
 * Remmina - The GTK+ Remote Desktop Client
 * Copyright (C) 2012-2012 Jean-Louis Dupond
 * Copyright (C) 2014-2015 Antenore Gatta, Fabio Castelli, Giovanni Panozzo
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 *
 *  In addition, as a special exception, the copyright holders give
 *  permission to link the code of portions of this program with the
 *  OpenSSL library under certain conditions as described in each
 *  individual source file, and distribute linked combinations
 *  including the two.
 *  You must obey the GNU General Public License in all respects
 *  for all of the code used other than OpenSSL. *  If you modify
 *  file(s) with this exception, you may extend this exception to your
 *  version of the file(s), but you are not obligated to do so. *  If you
 *  do not wish to do so, delete this exception statement from your
 *  version. *  If you delete this exception statement from all source
 *  files in the program, then also delete it here.
 *
 */

#include "rdp_plugin.h"
#include "rdp_cliprdr.h"
#include "rdp_event.h"

#include <freerdp/freerdp.h>
#include <freerdp/channels/channels.h>
#include <freerdp/client/cliprdr.h>
#include <sys/time.h>

#define CLIPBOARD_TRANSFER_WAIT_TIME 2

UINT32 remmina_rdp_cliprdr_get_format_from_gdkatom(GdkAtom atom)
{
	TRACE_CALL("remmina_rdp_cliprdr_get_format_from_gdkatom");
	gchar* name = gdk_atom_name(atom);
	if (g_strcmp0("UTF8_STRING", name) == 0 || g_strcmp0("text/plain;charset=utf-8", name) == 0)
	{
		return CF_UNICODETEXT;
	}
	if (g_strcmp0("TEXT", name) == 0 || g_strcmp0("text/plain", name) == 0)
	{
		return CF_TEXT;
	}
	if (g_strcmp0("text/html", name) == 0)
	{
		return CB_FORMAT_HTML;
	}
	if (g_strcmp0("image/png", name) == 0)
	{
		return CB_FORMAT_PNG;
	}
	if (g_strcmp0("image/jpeg", name) == 0)
	{
		return CB_FORMAT_JPEG;
	}
	if (g_strcmp0("image/bmp", name) == 0)
	{
		return CF_DIB;
	}
	return 0;
}

void remmina_rdp_cliprdr_get_target_types(UINT32** formats, UINT16* size, GdkAtom* types, int count)
{
	TRACE_CALL("remmina_rdp_cliprdr_get_target_types");
	int i;
	*size = 1;
	*formats = (UINT32*) malloc(sizeof(UINT32) * (count+1));

	*formats[0] = 0;
	for (i = 0; i < count; i++)
	{
		UINT32 format = remmina_rdp_cliprdr_get_format_from_gdkatom(types[i]);
		if (format != 0)
		{
			(*formats)[*size] = format;
			(*size)++;
		}
	}

	*formats = realloc(*formats, sizeof(UINT32) * (*size));
}

static UINT8* lf2crlf(UINT8* data, int* size)
{
	TRACE_CALL("lf2crlf");
	UINT8 c;
	UINT8* outbuf;
	UINT8* out;
	UINT8* in_end;
	UINT8* in;
	int out_size;

	out_size = (*size) * 2 + 1;
	outbuf = (UINT8*) malloc(out_size);
	out = outbuf;
	in = data;
	in_end = data + (*size);

	while (in < in_end)
	{
		c = *in++;
		if (c == '\n')
		{
			*out++ = '\r';
			*out++ = '\n';
		}
		else
		{
			*out++ = c;
		}
	}

	*out++ = 0;
	*size = out - outbuf;

	return outbuf;
}

static void crlf2lf(UINT8* data, size_t* size)
{
	TRACE_CALL("crlf2lf");
	UINT8 c;
	UINT8* out;
	UINT8* in;
	UINT8* in_end;

	out = data;
	in = data;
	in_end = data + (*size);

	while (in < in_end)
	{
		c = *in++;
		if (c != '\r')
			*out++ = c;
	}

	*size = out - data;
}

int remmina_rdp_cliprdr_server_file_contents_request(CliprdrClientContext* context, CLIPRDR_FILE_CONTENTS_REQUEST* fileContentsRequest)
{
	TRACE_CALL("remmina_rdp_cliprdr_server_file_contents_request");
/*	UINT32 uSize = 0;
	BYTE* pData = NULL;
	HRESULT hRet = S_OK;
	FORMATETC vFormatEtc;
	LPDATAOBJECT pDataObj = NULL;
	STGMEDIUM vStgMedium;
	LPSTREAM pStream = NULL;
	BOOL bIsStreamFile = TRUE;
	static LPSTREAM pStreamStc = NULL;
	static UINT32 uStreamIdStc = 0;
	wfClipboard* clipboard = (wfClipboard*) context->custom;
	if (fileContentsRequest->dwFlags == FILECONTENTS_SIZE)
	fileContentsRequest->cbRequested = sizeof(UINT64);
	pData = (BYTE*) calloc(1, fileContentsRequest->cbRequested);
	if (!pData)
	goto error;
	hRet = OleGetClipboard(&pDataObj);
	if (!SUCCEEDED(hRet))
	{
	WLog_ERR(TAG, "filecontents: get ole clipboard failed.");
	goto error;
	}
	ZeroMemory(&vFormatEtc, sizeof(FORMATETC));
	ZeroMemory(&vStgMedium, sizeof(STGMEDIUM));
	vFormatEtc.cfFormat = clipboard->ID_FILECONTENTS;
	vFormatEtc.tymed = TYMED_ISTREAM;
	vFormatEtc.dwAspect = 1;
	vFormatEtc.lindex = fileContentsRequest->listIndex;
	vFormatEtc.ptd = NULL;
	if ((uStreamIdStc != fileContentsRequest->streamId) || !pStreamStc)
	{
	LPENUMFORMATETC pEnumFormatEtc;
	ULONG CeltFetched;
	FORMATETC vFormatEtc2;
	if (pStreamStc)
	{
	IStream_Release(pStreamStc);
	pStreamStc = NULL;
	}
	bIsStreamFile = FALSE;
	hRet = IDataObject_EnumFormatEtc(pDataObj, DATADIR_GET, &pEnumFormatEtc);
	if (hRet == S_OK)
	{
	do
	{
	hRet = IEnumFORMATETC_Next(pEnumFormatEtc, 1, &vFormatEtc2, &CeltFetched);
	if (hRet == S_OK)
	{
	if (vFormatEtc2.cfFormat == clipboard->ID_FILECONTENTS)
	{
	hRet = IDataObject_GetData(pDataObj, &vFormatEtc, &vStgMedium);
	if (hRet == S_OK)
	{
	pStreamStc = vStgMedium.pstm;
	uStreamIdStc = fileContentsRequest->streamId;
	bIsStreamFile = TRUE;
	}
	break;
	}
	}
	}
	while (hRet == S_OK);
	}
	}
	if (bIsStreamFile == TRUE)
	{
	if (fileContentsRequest->dwFlags == FILECONTENTS_SIZE)
	{
	STATSTG vStatStg;
	ZeroMemory(&vStatStg, sizeof(STATSTG));
	hRet = IStream_Stat(pStreamStc, &vStatStg, STATFLAG_NONAME);
	if (hRet == S_OK)
	{
	*((UINT32*) &pData[0]) = vStatStg.cbSize.LowPart;
	*((UINT32*) &pData[4]) = vStatStg.cbSize.HighPart;
	uSize = fileContentsRequest->cbRequested;
	}
	}
	else if (fileContentsRequest->dwFlags == FILECONTENTS_RANGE)
	{
	LARGE_INTEGER dlibMove;
	ULARGE_INTEGER dlibNewPosition;
	dlibMove.HighPart = fileContentsRequest->nPositionHigh;
	dlibMove.LowPart = fileContentsRequest->nPositionLow;
	hRet = IStream_Seek(pStreamStc, dlibMove, STREAM_SEEK_SET, &dlibNewPosition);
	if (SUCCEEDED(hRet))
	{
	hRet = IStream_Read(pStreamStc, pData, fileContentsRequest->cbRequested, (PULONG) &uSize);
	}
	}
	}
	else
	{
	if (fileContentsRequest->dwFlags == FILECONTENTS_SIZE)
	{
	*((UINT32*) &pData[0]) = clipboard->fileDescriptor[fileContentsRequest->listIndex]->nFileSizeLow;
	*((UINT32*) &pData[4]) = clipboard->fileDescriptor[fileContentsRequest->listIndex]->nFileSizeHigh;
	uSize = fileContentsRequest->cbRequested;
	}
	else if (fileContentsRequest->dwFlags == FILECONTENTS_RANGE)
	{
	BOOL bRet;
	bRet = wf_cliprdr_get_file_contents(clipboard->file_names[fileContentsRequest->listIndex], pData,
	fileContentsRequest->nPositionLow, fileContentsRequest->nPositionHigh,
	fileContentsRequest->cbRequested, &uSize);
	if (bRet == FALSE)
	{
	WLog_ERR(TAG, "get file contents failed.");
	uSize = 0;
	goto error;
	}
	}
	}
	IDataObject_Release(pDataObj);
	if (uSize == 0)
	{
	free(pData);
	pData = NULL;
	}
	cliprdr_send_response_filecontents(clipboard, fileContentsRequest->streamId, uSize, pData);
	free(pData);
	return 1;
	error:
	if (pData)
	{
	free(pData);
	pData = NULL;
	}
	if (pDataObj)
	{
	IDataObject_Release(pDataObj);
	pDataObj = NULL;
	}
	WLog_ERR(TAG, "filecontents: send failed response.");
	cliprdr_send_response_filecontents(clipboard, fileContentsRequest->streamId, 0, NULL);
	*/
	return -1;
}
int remmina_rdp_cliprdr_server_file_contents_response(CliprdrClientContext* context, CLIPRDR_FILE_CONTENTS_RESPONSE* fileContentsResponse)
{
	TRACE_CALL("remmina_rdp_cliprdr_server_file_contents_response");
	/*
	wfClipboard* clipboard = (wfClipboard*) context->custom;
	clipboard->req_fsize = fileContentsResponse->cbRequested;
	clipboard->req_fdata = (char*) malloc(fileContentsResponse->cbRequested);
	CopyMemory(clipboard->req_fdata, fileContentsResponse->requestedData, fileContentsResponse->cbRequested);
	SetEvent(clipboard->req_fevent);
	*/
	return 1;
}


static UINT remmina_rdp_cliprdr_monitor_ready(CliprdrClientContext* context, CLIPRDR_MONITOR_READY* monitorReady)
{
	TRACE_CALL("remmina_rdp_cliprdr_monitor_ready");
	RemminaPluginRdpUiObject* ui;
	rfClipboard* clipboard = (rfClipboard*)context->custom;
	RemminaProtocolWidget* gp;

	gp = clipboard->rfi->protocol_widget;

	ui = g_new0(RemminaPluginRdpUiObject, 1);
	ui->type = REMMINA_RDP_UI_CLIPBOARD;
	ui->clipboard.clipboard = clipboard;
	ui->clipboard.type = REMMINA_RDP_UI_CLIPBOARD_MONITORREADY;
	ui->sync = TRUE;
	remmina_rdp_event_queue_ui(gp, ui);

	return CHANNEL_RC_OK;
}

static UINT remmina_rdp_cliprdr_server_capabilities(CliprdrClientContext* context, CLIPRDR_CAPABILITIES* capabilities)
{
	TRACE_CALL("remmina_rdp_cliprdr_server_capabilities");
	return CHANNEL_RC_OK;
}


static UINT remmina_rdp_cliprdr_server_format_list(CliprdrClientContext* context, CLIPRDR_FORMAT_LIST* formatList)
{
	TRACE_CALL("remmina_rdp_cliprdr_server_format_list");

	/* Called when a user do a "Copy" on the server: we collect all formats
	 * the server send us and then setup the local clipboard with the appropiate
	 * functions to request server data */

	RemminaPluginRdpUiObject* ui;
	RemminaProtocolWidget* gp;
	rfClipboard* clipboard;
	CLIPRDR_FORMAT* format;

	int i;

	clipboard = (rfClipboard*)context->custom;
	gp = clipboard->rfi->protocol_widget;
	GtkTargetList* list = gtk_target_list_new (NULL, 0);

	for (i = 0; i < formatList->numFormats; i++)
	{
		format = &formatList->formats[i];
		if (format->formatId == CF_UNICODETEXT)
		{
			GdkAtom atom = gdk_atom_intern("UTF8_STRING", TRUE);
			gtk_target_list_add(list, atom, 0, CF_UNICODETEXT);
		}
		else if (format->formatId == CF_TEXT)
		{
			GdkAtom atom = gdk_atom_intern("TEXT", TRUE);
			gtk_target_list_add(list, atom, 0, CF_TEXT);
		}
		else if (format->formatId == CF_DIB)
		{
			GdkAtom atom = gdk_atom_intern("image/bmp", TRUE);
			gtk_target_list_add(list, atom, 0, CF_DIB);
		}
		else if (format->formatId == CF_DIBV5)
		{
			GdkAtom atom = gdk_atom_intern("image/bmp", TRUE);
			gtk_target_list_add(list, atom, 0, CF_DIBV5);
		}
		else if (format->formatId == CB_FORMAT_JPEG)
		{
			GdkAtom atom = gdk_atom_intern("image/jpeg", TRUE);
			gtk_target_list_add(list, atom, 0, CB_FORMAT_JPEG);
		}
		else if (format->formatId == CB_FORMAT_PNG)
		{
			GdkAtom atom = gdk_atom_intern("image/png", TRUE);
			gtk_target_list_add(list, atom, 0, CB_FORMAT_PNG);
		}
		else if (format->formatId == CB_FORMAT_HTML)
		{
			GdkAtom atom = gdk_atom_intern("text/html", TRUE);
			gtk_target_list_add(list, atom, 0, CB_FORMAT_HTML);
		}
	}

	ui = g_new0(RemminaPluginRdpUiObject, 1);
	ui->type = REMMINA_RDP_UI_CLIPBOARD;
	ui->clipboard.clipboard = clipboard;
	ui->clipboard.type = REMMINA_RDP_UI_CLIPBOARD_SET_DATA;
	ui->clipboard.targetlist = list;
	ui->sync = TRUE;
	remmina_rdp_event_queue_ui(gp, ui);

	return CHANNEL_RC_OK;
}

static UINT remmina_rdp_cliprdr_server_format_list_response(CliprdrClientContext* context, CLIPRDR_FORMAT_LIST_RESPONSE* formatListResponse)
{
	TRACE_CALL("remmina_rdp_cliprdr_server_format_list_response");
	return CHANNEL_RC_OK;
}


static UINT remmina_rdp_cliprdr_server_format_data_request(CliprdrClientContext* context, CLIPRDR_FORMAT_DATA_REQUEST* formatDataRequest)
{
	TRACE_CALL("remmina_rdp_cliprdr_server_format_data_request");

	RemminaPluginRdpUiObject* ui;
	RemminaProtocolWidget* gp;
	rfClipboard* clipboard;

	clipboard = (rfClipboard*)context->custom;
	gp = clipboard->rfi->protocol_widget;

	ui = g_new0(RemminaPluginRdpUiObject, 1);
	ui->type = REMMINA_RDP_UI_CLIPBOARD;
	ui->clipboard.clipboard = clipboard;
	ui->clipboard.type = REMMINA_RDP_UI_CLIPBOARD_GET_DATA;
	ui->clipboard.format = formatDataRequest->requestedFormatId;
	ui->sync = TRUE;
	remmina_rdp_event_queue_ui(gp, ui);

	return CHANNEL_RC_OK;
}

static UINT remmina_rdp_cliprdr_server_format_data_response(CliprdrClientContext* context, CLIPRDR_FORMAT_DATA_RESPONSE* formatDataResponse)
{
	TRACE_CALL("remmina_rdp_cliprdr_server_format_data_response");

	UINT8* data;
	size_t size;
	rfContext* rfi;
	RemminaProtocolWidget* gp;
	rfClipboard* clipboard;
	GdkPixbufLoader *pixbuf;
	gpointer output = NULL;
	RemminaPluginRdpUiObject *ui;

	clipboard = (rfClipboard*)context->custom;
	gp = clipboard->rfi->protocol_widget;
	rfi = GET_PLUGIN_DATA(gp);

	data = formatDataResponse->requestedFormatData;
	size = formatDataResponse->dataLen;

	// formatDataResponse->requestedFormatData is allocated
	//  by freerdp and freed after returning from this callback function.
	//  So we must make a copy if we need to preserve it

	if (size > 0)
	{
		switch (rfi->clipboard.format)
		{
			case CF_UNICODETEXT:
			{
				size = ConvertFromUnicode(CP_UTF8, 0, (WCHAR*)data, size / 2, (CHAR**)&output, 0, NULL, NULL);
				crlf2lf(output, &size);
				break;
			}

			case CF_TEXT:
			case CB_FORMAT_HTML:
			{
				output = (gpointer)calloc(1, size + 1);
				if (output) {
					memcpy(output, data, size);
					crlf2lf(output, &size);
				}
				break;
			}

			case CF_DIBV5:
			case CF_DIB:
			{
				wStream* s;
				UINT32 offset;
				GError *perr;
				BITMAPINFOHEADER* pbi;
				BITMAPV5HEADER* pbi5;

				pbi = (BITMAPINFOHEADER*)data;

				// offset calculation inspired by http://downloads.poolelan.com/MSDN/MSDNLibrary6/Disk1/Samples/VC/OS/WindowsXP/GetImage/BitmapUtil.cpp
				offset = 14 + pbi->biSize;
				if (pbi->biClrUsed != 0)
					offset += sizeof(RGBQUAD) * pbi->biClrUsed;
				else if (pbi->biBitCount <= 8)
					offset += sizeof(RGBQUAD) * (1 << pbi->biBitCount);
				if (pbi->biSize == sizeof(BITMAPINFOHEADER)) {
					if (pbi->biCompression == 3) // BI_BITFIELDS is 3
							offset += 12;
				} else if (pbi->biSize >= sizeof(BITMAPV5HEADER)) {
					pbi5 = (BITMAPV5HEADER*)pbi;
					if (pbi5->bV5ProfileData <= offset)
							offset += pbi5->bV5ProfileSize;
				}
				s = Stream_New(NULL, 14 + size);
				Stream_Write_UINT8(s, 'B');
				Stream_Write_UINT8(s, 'M');
				Stream_Write_UINT32(s, 14 + size);
				Stream_Write_UINT32(s, 0);
				Stream_Write_UINT32(s, offset);
				Stream_Write(s, data, size);

				data = Stream_Buffer(s);
				size = Stream_Length(s);

				pixbuf = gdk_pixbuf_loader_new();
				perr = NULL;
				if ( !gdk_pixbuf_loader_write(pixbuf, data, size, &perr) ) {
						remmina_plugin_service->log_printf("[RDP] rdp_cliprdr: gdk_pixbuf_loader_write() returned error %s\n", perr->message);
				}
				else
				{
					if ( !gdk_pixbuf_loader_close(pixbuf, &perr) ) {
						remmina_plugin_service->log_printf("[RDP] rdp_cliprdr: gdk_pixbuf_loader_close() returned error %s\n", perr->message);
						perr = NULL;
					}
					Stream_Free(s, TRUE);
					output = g_object_ref(gdk_pixbuf_loader_get_pixbuf(pixbuf));
				}
				g_object_unref(pixbuf);
				break;
			}

			case CB_FORMAT_PNG:
			case CB_FORMAT_JPEG:
			{
				pixbuf = gdk_pixbuf_loader_new();
				gdk_pixbuf_loader_write(pixbuf, data, size, NULL);
				output = g_object_ref(gdk_pixbuf_loader_get_pixbuf(pixbuf));
				gdk_pixbuf_loader_close(pixbuf, NULL);
				g_object_unref(pixbuf);
				break;
			}
		}
	}

	pthread_mutex_lock(&clipboard->transfer_clip_mutex);
	pthread_cond_signal(&clipboard->transfer_clip_cond);
	if ( clipboard->srv_clip_data_wait == SCDW_BUSY_WAIT ) {
		clipboard->srv_data = output;
	}
	else
	{
		// Clipboard data arrived from server when we are not busywaiting.
		// Just put it on the local clipboard

		ui = g_new0(RemminaPluginRdpUiObject, 1);
		ui->type = REMMINA_RDP_UI_CLIPBOARD;
		ui->clipboard.clipboard = clipboard;
		ui->clipboard.type = REMMINA_RDP_UI_CLIPBOARD_SET_CONTENT;
		ui->clipboard.data = output;
		ui->clipboard.format = clipboard->format;
		remmina_rdp_event_queue_ui(gp, ui);

		clipboard->srv_clip_data_wait = SCDW_NONE;

	}
	pthread_mutex_unlock(&clipboard->transfer_clip_mutex);

	return CHANNEL_RC_OK;
}

void remmina_rdp_cliprdr_request_data(GtkClipboard *gtkClipboard, GtkSelectionData *selection_data, guint info, RemminaProtocolWidget* gp )
{
	TRACE_CALL("remmina_rdp_cliprdr_request_data");
	/* Called when someone press "Paste" on the client side.
	 * We ask to the server the data we need */

	GdkAtom target;
	CLIPRDR_FORMAT_DATA_REQUEST request;
	rfClipboard* clipboard;
	rfContext* rfi = GET_PLUGIN_DATA(gp);
	struct timespec to;
	struct timeval tv;
	int rc;

	clipboard = &(rfi->clipboard);
	if ( clipboard->srv_clip_data_wait != SCDW_NONE ) {
		remmina_plugin_service->log_printf("[RDP] Cannot paste now, I'm transferring clipboard data from server. Try again later\n");
		return;
	}


	target = gtk_selection_data_get_target(selection_data);
	// clipboard->format = remmina_rdp_cliprdr_get_format_from_gdkatom(target);
	clipboard->format = info;


	/* Request Clipboard content from the server */
	ZeroMemory(&request, sizeof(CLIPRDR_FORMAT_DATA_REQUEST));
	request.requestedFormatId = clipboard->format;
	request.msgFlags = CB_RESPONSE_OK;
	request.msgType = CB_FORMAT_DATA_REQUEST;


	pthread_mutex_lock(&clipboard->transfer_clip_mutex);

	clipboard->srv_clip_data_wait = SCDW_BUSY_WAIT;
	clipboard->context->ClientFormatDataRequest(clipboard->context, &request);

	/* Busy wait clibpoard data for CLIPBOARD_TRANSFER_WAIT_TIME seconds */
	gettimeofday(&tv, NULL);
	to.tv_sec = tv.tv_sec + CLIPBOARD_TRANSFER_WAIT_TIME;
	to.tv_nsec = tv.tv_usec * 1000;
	rc = pthread_cond_timedwait(&clipboard->transfer_clip_cond,&clipboard->transfer_clip_mutex, &to);

	if ( rc == 0 ) {
		/* Data has arrived without timeout */
		if (clipboard->srv_data != NULL)
		{
			if (info == CB_FORMAT_PNG || info == CF_DIB || info == CF_DIBV5 || info == CB_FORMAT_JPEG)
			{
				gtk_selection_data_set_pixbuf(selection_data, clipboard->srv_data);
				g_object_unref(clipboard->srv_data);
			}
			else
			{
				gtk_selection_data_set_text(selection_data, clipboard->srv_data, -1);
				free(clipboard->srv_data);
			}
		}
		clipboard->srv_clip_data_wait = SCDW_NONE;
	} else {
		clipboard->srv_clip_data_wait = SCDW_ASYNCWAIT;
		if ( rc == ETIMEDOUT ) {
			remmina_plugin_service->log_printf("[RDP] Clipboard data has not been transfered from the server in %d seconds. Try to paste later.\n",
				CLIPBOARD_TRANSFER_WAIT_TIME);
		}
		else {
			remmina_plugin_service->log_printf("[RDP] internal error: pthread_cond_timedwait() returned %d\n",rc);
			clipboard->srv_clip_data_wait = SCDW_NONE;
		}
	}
	pthread_mutex_unlock(&clipboard->transfer_clip_mutex);

}

void remmina_rdp_cliprdr_empty_clipboard(GtkClipboard *gtkClipboard, rfClipboard *clipboard)
{
	TRACE_CALL("remmina_rdp_cliprdr_empty_clipboard");
	/* No need to do anything here */
}

int remmina_rdp_cliprdr_send_client_capabilities(rfClipboard* clipboard)
{
	TRACE_CALL("remmina_rdp_cliprdr_send_client_capabilities");
	CLIPRDR_CAPABILITIES capabilities;
	CLIPRDR_GENERAL_CAPABILITY_SET generalCapabilitySet;

	capabilities.cCapabilitiesSets = 1;
	capabilities.capabilitySets = (CLIPRDR_CAPABILITY_SET*) &(generalCapabilitySet);

	generalCapabilitySet.capabilitySetType = CB_CAPSTYPE_GENERAL;
	generalCapabilitySet.capabilitySetLength = 12;

	generalCapabilitySet.version = CB_CAPS_VERSION_2;
	generalCapabilitySet.generalFlags = CB_USE_LONG_FORMAT_NAMES;

	clipboard->context->ClientCapabilities(clipboard->context, &capabilities);

	return 1;
}

int remmina_rdp_cliprdr_mt_send_format_list(RemminaProtocolWidget* gp, RemminaPluginRdpUiObject* ui)
{
	TRACE_CALL("remmina_rdp_cliprdr_mt_send_format_list");
	GtkClipboard* gtkClipboard;
	rfClipboard* clipboard;
	rfContext* rfi = GET_PLUGIN_DATA(gp);
	GdkAtom* targets;
	gboolean result = 0;
	gint loccount, srvcount;
	gint formatId, i;
	CLIPRDR_FORMAT_LIST formatList;
	CLIPRDR_FORMAT* formats;
	CLIPRDR_FORMAT* formats_new;

	clipboard = ui->clipboard.clipboard;
	formatList.formats = formats = NULL;
	formatList.numFormats = 0;

	if (clipboard->clipboard_wait) {
		clipboard->clipboard_wait = FALSE;
		return 0;
	}

	gtkClipboard = gtk_widget_get_clipboard(rfi->drawing_area, GDK_SELECTION_CLIPBOARD);
	if (gtkClipboard) {
		result = gtk_clipboard_wait_for_targets(gtkClipboard, &targets, &loccount);
	}

	if (result) {
		formats = (CLIPRDR_FORMAT*)malloc(loccount * sizeof(CLIPRDR_FORMAT));
		srvcount = 0;
		for(i = 0 ; i < loccount ; i++)  {
			formatId = remmina_rdp_cliprdr_get_format_from_gdkatom(targets[i]);
			if ( formatId != 0 ) {
				formats[srvcount].formatId = formatId;
				formats[srvcount].formatName = NULL;
				srvcount ++;
			}
		}
		formats_new = (CLIPRDR_FORMAT*)realloc(formats, srvcount * sizeof(CLIPRDR_FORMAT));
		if (formats_new == NULL) {
			printf("realloc failure in remmina_rdp_cliprdr_mt_send_format_list\n");
		} else {
			formats = formats_new;
		}
		g_free(targets);

		formatList.formats = formats;
		formatList.numFormats = srvcount;
	}

	formatList.msgFlags = CB_RESPONSE_OK;
	clipboard->context->ClientFormatList(clipboard->context, &formatList);

	if (formats)
		free(formats);

	return 1;
}


static void remmina_rdp_cliprdr_send_data_response(rfClipboard* clipboard, BYTE* data, int size)
{
	TRACE_CALL("remmina_rdp_cliprdr_send_data_response");
	CLIPRDR_FORMAT_DATA_RESPONSE response;

	ZeroMemory(&response, sizeof(CLIPRDR_FORMAT_DATA_RESPONSE));

	response.msgFlags = CB_RESPONSE_OK;
	response.dataLen = size;
	response.requestedFormatData = data;
	clipboard->context->ClientFormatDataResponse(clipboard->context, &response);
}


int remmina_rdp_cliprdr_mt_monitor_ready(RemminaProtocolWidget* gp, RemminaPluginRdpUiObject* ui)
{
	TRACE_CALL("remmina_rdp_cliprdr_mt_monitor_ready");
	rfClipboard *clipboard = ui->clipboard.clipboard;

	if ( clipboard->clipboard_wait )
	{
		clipboard->clipboard_wait = FALSE;
		return 0;
	}

	remmina_rdp_cliprdr_send_client_capabilities(clipboard);
	remmina_rdp_cliprdr_mt_send_format_list(gp,ui);

	clipboard->sync = TRUE;
	return 1;
}


void remmina_rdp_cliprdr_get_clipboard_data(RemminaProtocolWidget* gp, RemminaPluginRdpUiObject* ui)
{
	TRACE_CALL("remmina_rdp_cliprdr_get_clipboard_data");
	GtkClipboard* gtkClipboard;
	rfClipboard* clipboard;
	UINT8* inbuf = NULL;
	UINT8* outbuf = NULL;
	GdkPixbuf *image = NULL;
	int size = 0;
	rfContext* rfi = GET_PLUGIN_DATA(gp);

	clipboard = ui->clipboard.clipboard;
	gtkClipboard = gtk_widget_get_clipboard(rfi->drawing_area, GDK_SELECTION_CLIPBOARD);
	if (gtkClipboard)
	{
		switch (ui->clipboard.format)
		{
			case CF_TEXT:
			case CF_UNICODETEXT:
			case CB_FORMAT_HTML:
			{
				inbuf = (UINT8*)gtk_clipboard_wait_for_text(gtkClipboard);
				break;
			}

			case CB_FORMAT_PNG:
			case CB_FORMAT_JPEG:
			case CF_DIB:
			case CF_DIBV5:
			{
				image = gtk_clipboard_wait_for_image(gtkClipboard);
				break;
			}
		}
	}

	/* No data received, send nothing */
	if (inbuf != NULL || image != NULL)
	{
		switch (ui->clipboard.format)
		{
			case CF_TEXT:
			case CB_FORMAT_HTML:
			{
				size = strlen((char*)inbuf);
				outbuf = lf2crlf(inbuf, &size);
				break;
			}
			case CF_UNICODETEXT:
			{
				size = strlen((char*)inbuf);
				inbuf = lf2crlf(inbuf, &size);
				size = (ConvertToUnicode(CP_UTF8, 0, (CHAR*)inbuf, -1, (WCHAR**)&outbuf, 0) ) * sizeof(WCHAR);
				g_free(inbuf);
				break;
			}
			case CB_FORMAT_PNG:
			{
				gchar* data;
				gsize buffersize;
				gdk_pixbuf_save_to_buffer(image, &data, &buffersize, "png", NULL, NULL);
				outbuf = (UINT8*) malloc(buffersize);
				memcpy(outbuf, data, buffersize);
				size = buffersize;
				g_object_unref(image);
				break;
			}
			case CB_FORMAT_JPEG:
			{
				gchar* data;
				gsize buffersize;
				gdk_pixbuf_save_to_buffer(image, &data, &buffersize, "jpeg", NULL, NULL);
				outbuf = (UINT8*) malloc(buffersize);
				memcpy(outbuf, data, buffersize);
				size = buffersize;
				g_object_unref(image);
				break;
			}
			case CF_DIB:
			case CF_DIBV5:
			{
				gchar* data;
				gsize buffersize;
				gdk_pixbuf_save_to_buffer(image, &data, &buffersize, "bmp", NULL, NULL);
				size = buffersize - 14;
				outbuf = (UINT8*) malloc(size);
				memcpy(outbuf, data + 14, size);
				g_object_unref(image);
				break;
			}
		}
	}
	remmina_rdp_cliprdr_send_data_response(clipboard, outbuf, size);
}
void remmina_rdp_cliprdr_set_clipboard_content(RemminaProtocolWidget* gp, RemminaPluginRdpUiObject* ui)
{
	TRACE_CALL("remmina_rdp_cliprdr_set_clipboard_content");
	GtkClipboard* gtkClipboard;
	rfContext* rfi = GET_PLUGIN_DATA(gp);

	gtkClipboard = gtk_widget_get_clipboard(rfi->drawing_area, GDK_SELECTION_CLIPBOARD);
	if (ui->clipboard.format == CB_FORMAT_PNG || ui->clipboard.format == CF_DIB || ui->clipboard.format == CF_DIBV5 || ui->clipboard.format == CB_FORMAT_JPEG) {
		gtk_clipboard_set_image( gtkClipboard, ui->clipboard.data );
		g_object_unref(ui->clipboard.data);
	}
	else {
		gtk_clipboard_set_text( gtkClipboard, ui->clipboard.data, -1 );
		free(ui->clipboard.data);
	}

}

void remmina_rdp_cliprdr_set_clipboard_data(RemminaProtocolWidget* gp, RemminaPluginRdpUiObject* ui)
{
	TRACE_CALL("remmina_rdp_cliprdr_set_clipboard_data");
	GtkClipboard* gtkClipboard;
	GtkTargetEntry* targets;
	gint n_targets;
	rfContext* rfi = GET_PLUGIN_DATA(gp);
	rfClipboard* clipboard;

	clipboard = ui->clipboard.clipboard;
	targets = gtk_target_table_new_from_list(ui->clipboard.targetlist, &n_targets);
	gtkClipboard = gtk_widget_get_clipboard(rfi->drawing_area, GDK_SELECTION_CLIPBOARD);
	if (gtkClipboard && targets)
	{
		clipboard->clipboard_wait = TRUE;
		gtk_clipboard_set_with_owner(gtkClipboard, targets, n_targets,
				(GtkClipboardGetFunc) remmina_rdp_cliprdr_request_data,
				(GtkClipboardClearFunc) remmina_rdp_cliprdr_empty_clipboard, G_OBJECT(gp));
		gtk_target_table_free(targets, n_targets);
	}
}

void remmina_rdp_event_process_clipboard(RemminaProtocolWidget* gp, RemminaPluginRdpUiObject* ui)
{
	TRACE_CALL("remmina_rdp_event_process_clipboard");
	switch (ui->clipboard.type)
	{

		case REMMINA_RDP_UI_CLIPBOARD_FORMATLIST:
			remmina_rdp_cliprdr_mt_send_format_list(gp, ui);
			break;
		case REMMINA_RDP_UI_CLIPBOARD_MONITORREADY:
			remmina_rdp_cliprdr_mt_monitor_ready(gp, ui);
			break;

		case REMMINA_RDP_UI_CLIPBOARD_GET_DATA:
			remmina_rdp_cliprdr_get_clipboard_data(gp, ui);
			break;

		case REMMINA_RDP_UI_CLIPBOARD_SET_DATA:
			remmina_rdp_cliprdr_set_clipboard_data(gp, ui);
			break;

		case REMMINA_RDP_UI_CLIPBOARD_SET_CONTENT:
			remmina_rdp_cliprdr_set_clipboard_content(gp, ui);
			break;

	}
}

void remmina_rdp_clipboard_init(rfContext *rfi)
{
	TRACE_CALL("remmina_rdp_clipboard_init");
	// Future: initialize rfi->clipboard
}
void remmina_rdp_clipboard_free(rfContext *rfi)
{
	TRACE_CALL("remmina_rdp_clipboard_free");
	// Future: deinitialize rfi->clipboard
}


void remmina_rdp_cliprdr_init(rfContext* rfi, CliprdrClientContext* cliprdr)
{
	TRACE_CALL("remmina_rdp_cliprdr_init");

	rfClipboard* clipboard;
	clipboard = &(rfi->clipboard);

	rfi->clipboard.rfi = rfi;
	cliprdr->custom = (void*)clipboard;

	clipboard->context = cliprdr;
	pthread_mutex_init(&clipboard->transfer_clip_mutex, NULL);
	pthread_cond_init(&clipboard->transfer_clip_cond,NULL);
	clipboard->srv_clip_data_wait = SCDW_NONE;

	cliprdr->MonitorReady = remmina_rdp_cliprdr_monitor_ready;
	cliprdr->ServerCapabilities = remmina_rdp_cliprdr_server_capabilities;
	cliprdr->ServerFormatList = remmina_rdp_cliprdr_server_format_list;
	cliprdr->ServerFormatListResponse = remmina_rdp_cliprdr_server_format_list_response;
	cliprdr->ServerFormatDataRequest = remmina_rdp_cliprdr_server_format_data_request;
	cliprdr->ServerFormatDataResponse = remmina_rdp_cliprdr_server_format_data_response;

//	cliprdr->ServerFileContentsRequest = remmina_rdp_cliprdr_server_file_contents_request;
//	cliprdr->ServerFileContentsResponse = remmina_rdp_cliprdr_server_file_contents_response;

}

