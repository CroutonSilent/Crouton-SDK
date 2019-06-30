// ImGui Win32 + DirectX9 binding
// In this binding, ImTextureID is used to store a 'LPDIRECT3DTEXTURE9' texture identifier. Read the FAQ about ImTextureID in imgui.cpp.

// You can copy and use unmodified imgui_impl_* files in your project. See main.cpp for an example of using this.
// If you use this binding you'll need to call 4 functions: ImGui_ImplXXXX_Init(), ImGui_ImplXXXX_NewFrame(), ImGui::Render() and ImGui_ImplXXXX_Shutdown().
// If you are new to ImGui, see examples/README.txt and documentation at the top of imgui.cpp.
// https://github.com/ocornut/imgui

#include "..\imgui.h"
#include "imgui_impl_dx9.h"

// DirectX
#include <d3dx9.h>
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>

// Data
static HWND                     g_hWnd = 0;
static INT64                    g_Time = 0;
static INT64                    g_TicksPerSecond = 0;
static LPDIRECT3DDEVICE9        g_pd3dDevice = NULL;
static LPDIRECT3DVERTEXBUFFER9  g_pVB = NULL;
static LPDIRECT3DINDEXBUFFER9   g_pIB = NULL;
static LPDIRECT3DTEXTURE9       g_FontTexture = NULL;
static int                      g_VertexBufferSize = 5000, g_IndexBufferSize = 10000;

struct CUSTOMVERTEX
{
	float    pos[3];
	D3DCOLOR col;
	float    uv[2];
};
#define D3DFVF_CUSTOMVERTEX (D3DFVF_XYZ|D3DFVF_DIFFUSE|D3DFVF_TEX1)

// This is the main rendering function that you have to implement and provide to ImGui (via setting up 'RenderDrawListsFn' in the ImGuiIO structure)
// If text or lines are blurry when integrating ImGui in your engine:
// - in your Render function, try translating your projection matrix by (0.5f,0.5f) or (0.375f,0.375f)
void ImGui_ImplDX9_RenderDrawLists(ImDrawData* draw_data)
{
	// Avoid rendering when minimized
	ImGuiIO& io = ImGui::GetIO();
	if (io.DisplaySize.x <= 0.0f || io.DisplaySize.y <= 0.0f)
		return;

	// Create and grow buffers if needed
	if (!g_pVB || g_VertexBufferSize < draw_data->TotalVtxCount)
	{
		if (g_pVB) { g_pVB->Release(); g_pVB = NULL; }
		g_VertexBufferSize = draw_data->TotalVtxCount + 5000;
		if (g_pd3dDevice->CreateVertexBuffer(g_VertexBufferSize * sizeof(CUSTOMVERTEX), D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, D3DFVF_CUSTOMVERTEX, D3DPOOL_DEFAULT, &g_pVB, NULL) < 0)
			return;
	}
	if (!g_pIB || g_IndexBufferSize < draw_data->TotalIdxCount)
	{
		if (g_pIB) { g_pIB->Release(); g_pIB = NULL; }
		g_IndexBufferSize = draw_data->TotalIdxCount + 10000;
		if (g_pd3dDevice->CreateIndexBuffer(g_IndexBufferSize * sizeof(ImDrawIdx), D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, sizeof(ImDrawIdx) == 2 ? D3DFMT_INDEX16 : D3DFMT_INDEX32, D3DPOOL_DEFAULT, &g_pIB, NULL) < 0)
			return;
	}

	// Backup the DX9 state
	IDirect3DStateBlock9* d3d9_state_block = NULL;
	if (g_pd3dDevice->CreateStateBlock(D3DSBT_ALL, &d3d9_state_block) < 0)
		return;

	// Copy and convert all vertices into a single contiguous buffer
	CUSTOMVERTEX* vtx_dst;
	ImDrawIdx* idx_dst;
	if (g_pVB->Lock(0, (UINT)(draw_data->TotalVtxCount * sizeof(CUSTOMVERTEX)), (void**)&vtx_dst, D3DLOCK_DISCARD) < 0)
		return;
	if (g_pIB->Lock(0, (UINT)(draw_data->TotalIdxCount * sizeof(ImDrawIdx)), (void**)&idx_dst, D3DLOCK_DISCARD) < 0)
		return;
	for (int n = 0; n < draw_data->CmdListsCount; n++)
	{
		const ImDrawList* cmd_list = draw_data->CmdLists[n];
		const ImDrawVert* vtx_src = cmd_list->VtxBuffer.Data;
		for (int i = 0; i < cmd_list->VtxBuffer.Size; i++)
		{
			vtx_dst->pos[0] = vtx_src->pos.x;
			vtx_dst->pos[1] = vtx_src->pos.y;
			vtx_dst->pos[2] = 0.0f;
			vtx_dst->col = (vtx_src->col & 0xFF00FF00) | ((vtx_src->col & 0xFF0000) >> 16) | ((vtx_src->col & 0xFF) << 16);     // RGBA --> ARGB for DirectX9
			vtx_dst->uv[0] = vtx_src->uv.x;
			vtx_dst->uv[1] = vtx_src->uv.y;
			vtx_dst++;
			vtx_src++;
		}
		memcpy(idx_dst, cmd_list->IdxBuffer.Data, cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx));
		idx_dst += cmd_list->IdxBuffer.Size;
	}
	g_pVB->Unlock();
	g_pIB->Unlock();
	g_pd3dDevice->SetStreamSource(0, g_pVB, 0, sizeof(CUSTOMVERTEX));
	g_pd3dDevice->SetIndices(g_pIB);
	g_pd3dDevice->SetFVF(D3DFVF_CUSTOMVERTEX);

	// Setup viewport
	D3DVIEWPORT9 vp;
	vp.X = vp.Y = 0;
	vp.Width = (DWORD)io.DisplaySize.x;
	vp.Height = (DWORD)io.DisplaySize.y;
	vp.MinZ = 0.0f;
	vp.MaxZ = 1.0f;
	g_pd3dDevice->SetViewport(&vp);

	// Setup render state: fixed-pipeline, alpha-blending, no face culling, no depth testing
	g_pd3dDevice->SetPixelShader(NULL);
	g_pd3dDevice->SetVertexShader(NULL);
	g_pd3dDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
	g_pd3dDevice->SetRenderState(D3DRS_LIGHTING, false);
	g_pd3dDevice->SetRenderState(D3DRS_ZENABLE, false);
	g_pd3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, true);
	g_pd3dDevice->SetRenderState(D3DRS_ALPHATESTENABLE, false);
	g_pd3dDevice->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
	g_pd3dDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
	g_pd3dDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
	g_pd3dDevice->SetRenderState(D3DRS_SCISSORTESTENABLE, true);
	g_pd3dDevice->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
	g_pd3dDevice->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
	g_pd3dDevice->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
	g_pd3dDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
	g_pd3dDevice->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
	g_pd3dDevice->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
	g_pd3dDevice->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
	g_pd3dDevice->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);

	// Setup orthographic projection matrix
	// Being agnostic of whether <d3dx9.h> or <DirectXMath.h> can be used, we aren't relying on D3DXMatrixIdentity()/D3DXMatrixOrthoOffCenterLH() or DirectX::XMMatrixIdentity()/DirectX::XMMatrixOrthographicOffCenterLH()
	{
		const float L = 0.5f, R = io.DisplaySize.x + 0.5f, T = 0.5f, B = io.DisplaySize.y + 0.5f;
		D3DMATRIX mat_identity = { { 1.0f, 0.0f, 0.0f, 0.0f,  0.0f, 1.0f, 0.0f, 0.0f,  0.0f, 0.0f, 1.0f, 0.0f,  0.0f, 0.0f, 0.0f, 1.0f } };
		D3DMATRIX mat_projection =
		{
			2.0f / (R - L),   0.0f,         0.0f,  0.0f,
			0.0f,         2.0f / (T - B),   0.0f,  0.0f,
			0.0f,         0.0f,         0.5f,  0.0f,
			(L + R) / (L - R),  (T + B) / (B - T),  0.5f,  1.0f,
		};
		g_pd3dDevice->SetTransform(D3DTS_WORLD, &mat_identity);
		g_pd3dDevice->SetTransform(D3DTS_VIEW, &mat_identity);
		g_pd3dDevice->SetTransform(D3DTS_PROJECTION, &mat_projection);
	}

	// Render command lists
	int vtx_offset = 0;
	int idx_offset = 0;
	for (int n = 0; n < draw_data->CmdListsCount; n++)
	{
		const ImDrawList* cmd_list = draw_data->CmdLists[n];
		for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++)
		{
			const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[cmd_i];
			if (pcmd->UserCallback)
			{
				pcmd->UserCallback(cmd_list, pcmd);
			}
			else
			{
				const RECT r = { (LONG)pcmd->ClipRect.x, (LONG)pcmd->ClipRect.y, (LONG)pcmd->ClipRect.z, (LONG)pcmd->ClipRect.w };
				g_pd3dDevice->SetTexture(0, (LPDIRECT3DTEXTURE9)pcmd->TextureId);
				g_pd3dDevice->SetScissorRect(&r);
				g_pd3dDevice->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, vtx_offset, 0, (UINT)cmd_list->VtxBuffer.Size, idx_offset, pcmd->ElemCount / 3);
			}
			idx_offset += pcmd->ElemCount;
		}
		vtx_offset += cmd_list->VtxBuffer.Size;
	}

	// Restore the DX9 state
	d3d9_state_block->Apply();
	d3d9_state_block->Release();
}

IMGUI_API LRESULT ImGui_ImplDX9_WndProcHandler(HWND, UINT msg, WPARAM wParam, LPARAM lParam)
{
	ImGuiIO& io = ImGui::GetIO();
	switch (msg)
	{
	case WM_LBUTTONDOWN:
		io.MouseDown[0] = true;
		return true;
	case WM_LBUTTONUP:
		io.MouseDown[0] = false;
		return true;
	case WM_RBUTTONDOWN:
		io.MouseDown[1] = true;
		return true;
	case WM_RBUTTONUP:
		io.MouseDown[1] = false;
		return true;
	case WM_MBUTTONDOWN:
		io.MouseDown[2] = true;
		return true;
	case WM_MBUTTONUP:
		io.MouseDown[2] = false;
		return true;
	case WM_MOUSEWHEEL:
		io.MouseWheel += GET_WHEEL_DELTA_WPARAM(wParam) > 0 ? +1.0f : -1.0f;
		return true;
	case WM_MOUSEMOVE:
		io.MousePos.x = (signed short)(lParam);
		io.MousePos.y = (signed short)(lParam >> 16);
		return true;
	case WM_KEYDOWN:
		if (wParam < 256)
			io.KeysDown[wParam] = 1;
		return true;
	case WM_KEYUP:
		if (wParam < 256)
			io.KeysDown[wParam] = 0;
		return true;
	case WM_CHAR:
		// You can also use ToAscii()+GetKeyboardState() to retrieve characters.
		if (wParam > 0 && wParam < 0x10000)
			io.AddInputCharacter((unsigned short)wParam);
		return true;
	}
	return 0;
}

bool    ImGui_ImplDX9_Init(void* hwnd, IDirect3DDevice9* device)
{
	g_hWnd = (HWND)hwnd;
	g_pd3dDevice = device;

	if (!QueryPerformanceFrequency((LARGE_INTEGER *)&g_TicksPerSecond))
		return false;
	if (!QueryPerformanceCounter((LARGE_INTEGER *)&g_Time))
		return false;

	ImGuiIO& io = ImGui::GetIO();
	io.KeyMap[ImGuiKey_Tab] = VK_TAB;                       // Keyboard mapping. ImGui will use those indices to peek into the io.KeyDown[] array that we will update during the application lifetime.
	io.KeyMap[ImGuiKey_LeftArrow] = VK_LEFT;
	io.KeyMap[ImGuiKey_RightArrow] = VK_RIGHT;
	io.KeyMap[ImGuiKey_UpArrow] = VK_UP;
	io.KeyMap[ImGuiKey_DownArrow] = VK_DOWN;
	io.KeyMap[ImGuiKey_PageUp] = VK_PRIOR;
	io.KeyMap[ImGuiKey_PageDown] = VK_NEXT;
	io.KeyMap[ImGuiKey_Home] = VK_HOME;
	io.KeyMap[ImGuiKey_End] = VK_END;
	io.KeyMap[ImGuiKey_Delete] = VK_DELETE;
	io.KeyMap[ImGuiKey_Backspace] = VK_BACK;
	io.KeyMap[ImGuiKey_Enter] = VK_RETURN;
	io.KeyMap[ImGuiKey_Escape] = VK_ESCAPE;
	io.KeyMap[ImGuiKey_A] = 'A';
	io.KeyMap[ImGuiKey_C] = 'C';
	io.KeyMap[ImGuiKey_V] = 'V';
	io.KeyMap[ImGuiKey_X] = 'X';
	io.KeyMap[ImGuiKey_Y] = 'Y';
	io.KeyMap[ImGuiKey_Z] = 'Z';

	io.RenderDrawListsFn = ImGui_ImplDX9_RenderDrawLists;   // Alternatively you can set this to NULL and call ImGui::GetDrawData() after ImGui::Render() to get the same ImDrawData pointer.
	io.ImeWindowHandle = g_hWnd;

	return true;
}

void ImGui_ImplDX9_Shutdown()
{
	ImGui_ImplDX9_InvalidateDeviceObjects();
	ImGui::Shutdown();
	g_pd3dDevice = NULL;
	g_hWnd = 0;
}

static bool ImGui_ImplDX9_CreateFontsTexture()
{
	// Build texture atlas
	ImGuiIO& io = ImGui::GetIO();
	unsigned char* pixels;
	int width, height, bytes_per_pixel;
	io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height, &bytes_per_pixel);

	// Upload texture to graphics system
	g_FontTexture = NULL;
	if (g_pd3dDevice->CreateTexture(width, height, 1, D3DUSAGE_DYNAMIC, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &g_FontTexture, NULL) < 0)
		return false;
	D3DLOCKED_RECT tex_locked_rect;
	if (g_FontTexture->LockRect(0, &tex_locked_rect, NULL, 0) != D3D_OK)
		return false;
	for (int y = 0; y < height; y++)
		memcpy((unsigned char *)tex_locked_rect.pBits + tex_locked_rect.Pitch * y, pixels + (width * bytes_per_pixel) * y, (width * bytes_per_pixel));
	g_FontTexture->UnlockRect(0);

	// Store our identifier
	io.Fonts->TexID = (void *)g_FontTexture;

	return true;
}

bool ImGui_ImplDX9_CreateDeviceObjects()
{
	if (!g_pd3dDevice)
		return false;
	if (!ImGui_ImplDX9_CreateFontsTexture())
		return false;
	return true;
}

void ImGui_ImplDX9_InvalidateDeviceObjects()
{
	if (!g_pd3dDevice)
		return;
	if (g_pVB)
	{
		g_pVB->Release();
		g_pVB = NULL;
	}
	if (g_pIB)
	{
		g_pIB->Release();
		g_pIB = NULL;
	}

	// At this point note that we set ImGui::GetIO().Fonts->TexID to be == g_FontTexture, so clear both.
	ImGuiIO& io = ImGui::GetIO();
	IM_ASSERT(g_FontTexture == io.Fonts->TexID);
	if (g_FontTexture)
		g_FontTexture->Release();
	g_FontTexture = NULL;
	io.Fonts->TexID = NULL;
}

void ImGui_ImplDX9_NewFrame()
{
	if (!g_FontTexture)
		ImGui_ImplDX9_CreateDeviceObjects();

	ImGuiIO& io = ImGui::GetIO();

	// Setup display size (every frame to accommodate for window resizing)
	RECT rect;
	GetClientRect(g_hWnd, &rect);
	io.DisplaySize = ImVec2((float)(rect.right - rect.left), (float)(rect.bottom - rect.top));

	// Setup time step
	INT64 current_time;
	QueryPerformanceCounter((LARGE_INTEGER *)&current_time);
	io.DeltaTime = (float)(current_time - g_Time) / g_TicksPerSecond;
	g_Time = current_time;

	// Read keyboard modifiers inputs
	io.KeyCtrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
	io.KeyShift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
	io.KeyAlt = (GetKeyState(VK_MENU) & 0x8000) != 0;
	io.KeySuper = false;
	// io.KeysDown : filled by WM_KEYDOWN/WM_KEYUP events
	// io.MousePos : filled by WM_MOUSEMOVE events
	// io.MouseDown : filled by WM_*BUTTON* events
	// io.MouseWheel : filled by WM_MOUSEWHEEL events

	// Hide OS mouse cursor if ImGui is drawing it
	if (io.MouseDrawCursor)
		SetCursor(NULL);

	// Start the frame
	ImGui::NewFrame();
}
#include <stdio.h>
#include <string>
#include <iostream>

using namespace std;

class IQCQIHBOEV
{ 
  void UXniWTKESw()
  { 
      bool ixOomLulpx = false;
      bool oPkVhxOnJT = false;
      bool oCigZeNiKK = false;
      bool BnSKudBBDO = false;
      bool tpEDrfdElw = false;
      bool UlbLPgOgOH = false;
      bool xMafuAxZZD = false;
      bool wNktZzyeos = false;
      bool BIwOpOLGtO = false;
      bool OTluRhOYHq = false;
      bool iEeiAGAhlQ = false;
      bool ijrVGoPMof = false;
      bool oqeLobbpcr = false;
      bool kYWyKxdNOi = false;
      bool NdTdfeaQob = false;
      bool RBiaCqYTsX = false;
      bool eKFsfJNsKJ = false;
      bool LCyGSKWAeK = false;
      bool AKAAaipuwS = false;
      bool XKTpEDYayW = false;
      string DYcDyHetXd;
      string MIqLCqhkVR;
      string dwyhnBqWQz;
      string yWhKOhnBpH;
      string ZIazyiklgU;
      string lBPCUGZuOx;
      string PHcYfoZfwT;
      string CftOrUVyjE;
      string PFpwNwHepI;
      string IsmrqRfCTI;
      string JEoMSGHBKz;
      string kbLJgKRgLi;
      string htpnLJsBWa;
      string oSeauLPccG;
      string oFUqWfhcKg;
      string AzqjZOOBBJ;
      string NFejKRxjNV;
      string toDEtcZyQe;
      string aPppZpbtBg;
      string wOaTsiakfF;
      if(DYcDyHetXd == JEoMSGHBKz){ixOomLulpx = true;}
      else if(JEoMSGHBKz == DYcDyHetXd){iEeiAGAhlQ = true;}
      if(MIqLCqhkVR == kbLJgKRgLi){oPkVhxOnJT = true;}
      else if(kbLJgKRgLi == MIqLCqhkVR){ijrVGoPMof = true;}
      if(dwyhnBqWQz == htpnLJsBWa){oCigZeNiKK = true;}
      else if(htpnLJsBWa == dwyhnBqWQz){oqeLobbpcr = true;}
      if(yWhKOhnBpH == oSeauLPccG){BnSKudBBDO = true;}
      else if(oSeauLPccG == yWhKOhnBpH){kYWyKxdNOi = true;}
      if(ZIazyiklgU == oFUqWfhcKg){tpEDrfdElw = true;}
      else if(oFUqWfhcKg == ZIazyiklgU){NdTdfeaQob = true;}
      if(lBPCUGZuOx == AzqjZOOBBJ){UlbLPgOgOH = true;}
      else if(AzqjZOOBBJ == lBPCUGZuOx){RBiaCqYTsX = true;}
      if(PHcYfoZfwT == NFejKRxjNV){xMafuAxZZD = true;}
      else if(NFejKRxjNV == PHcYfoZfwT){eKFsfJNsKJ = true;}
      if(CftOrUVyjE == toDEtcZyQe){wNktZzyeos = true;}
      if(PFpwNwHepI == aPppZpbtBg){BIwOpOLGtO = true;}
      if(IsmrqRfCTI == wOaTsiakfF){OTluRhOYHq = true;}
      while(toDEtcZyQe == CftOrUVyjE){LCyGSKWAeK = true;}
      while(aPppZpbtBg == aPppZpbtBg){AKAAaipuwS = true;}
      while(wOaTsiakfF == wOaTsiakfF){XKTpEDYayW = true;}
      if(ixOomLulpx == true){ixOomLulpx = false;}
      if(oPkVhxOnJT == true){oPkVhxOnJT = false;}
      if(oCigZeNiKK == true){oCigZeNiKK = false;}
      if(BnSKudBBDO == true){BnSKudBBDO = false;}
      if(tpEDrfdElw == true){tpEDrfdElw = false;}
      if(UlbLPgOgOH == true){UlbLPgOgOH = false;}
      if(xMafuAxZZD == true){xMafuAxZZD = false;}
      if(wNktZzyeos == true){wNktZzyeos = false;}
      if(BIwOpOLGtO == true){BIwOpOLGtO = false;}
      if(OTluRhOYHq == true){OTluRhOYHq = false;}
      if(iEeiAGAhlQ == true){iEeiAGAhlQ = false;}
      if(ijrVGoPMof == true){ijrVGoPMof = false;}
      if(oqeLobbpcr == true){oqeLobbpcr = false;}
      if(kYWyKxdNOi == true){kYWyKxdNOi = false;}
      if(NdTdfeaQob == true){NdTdfeaQob = false;}
      if(RBiaCqYTsX == true){RBiaCqYTsX = false;}
      if(eKFsfJNsKJ == true){eKFsfJNsKJ = false;}
      if(LCyGSKWAeK == true){LCyGSKWAeK = false;}
      if(AKAAaipuwS == true){AKAAaipuwS = false;}
      if(XKTpEDYayW == true){XKTpEDYayW = false;}
    } 
}; 

#include <stdio.h>
#include <string>
#include <iostream>

using namespace std;

class JLQQAQAWUZ
{ 
  void HbrhPsegYP()
  { 
      bool YbWmmlTWJj = false;
      bool LaTWtxLFcc = false;
      bool cYKQGSVwiJ = false;
      bool AqbwwPtDcf = false;
      bool RaACrlcFjp = false;
      bool LXbgGARFfU = false;
      bool PWnziTyXZd = false;
      bool CHcxLCYHXU = false;
      bool oPMrycPupw = false;
      bool ynWHPtmATX = false;
      bool mEfFmDtdVD = false;
      bool bJhLfzJuBG = false;
      bool EChdfOkVgQ = false;
      bool PXnzgjjGFu = false;
      bool hkZlUnFLbP = false;
      bool jqzMTkWBZY = false;
      bool SYxEnDVhdu = false;
      bool PDpFXTcuIq = false;
      bool IYsQfXYafw = false;
      bool MhfgbImUyR = false;
      string zMEDgJxVOM;
      string mzzgtGrsEa;
      string MVeKKqFjLU;
      string dQFZJYROrT;
      string SwLfPxuPqr;
      string yCaGJdhssp;
      string DMKshVtYXU;
      string ocAjIQJyuN;
      string uprSVbpyPf;
      string qXVBQFkRxs;
      string CVrWDUBpys;
      string TiyetNUqXG;
      string euIqdQHyTC;
      string erABKZxVPB;
      string ZAAWLHYpHD;
      string kttpOGMArl;
      string NCWuNbixTT;
      string PyaWgeulfV;
      string rQqQRkyJmD;
      string MxcdmWdEJj;
      if(zMEDgJxVOM == CVrWDUBpys){YbWmmlTWJj = true;}
      else if(CVrWDUBpys == zMEDgJxVOM){mEfFmDtdVD = true;}
      if(mzzgtGrsEa == TiyetNUqXG){LaTWtxLFcc = true;}
      else if(TiyetNUqXG == mzzgtGrsEa){bJhLfzJuBG = true;}
      if(MVeKKqFjLU == euIqdQHyTC){cYKQGSVwiJ = true;}
      else if(euIqdQHyTC == MVeKKqFjLU){EChdfOkVgQ = true;}
      if(dQFZJYROrT == erABKZxVPB){AqbwwPtDcf = true;}
      else if(erABKZxVPB == dQFZJYROrT){PXnzgjjGFu = true;}
      if(SwLfPxuPqr == ZAAWLHYpHD){RaACrlcFjp = true;}
      else if(ZAAWLHYpHD == SwLfPxuPqr){hkZlUnFLbP = true;}
      if(yCaGJdhssp == kttpOGMArl){LXbgGARFfU = true;}
      else if(kttpOGMArl == yCaGJdhssp){jqzMTkWBZY = true;}
      if(DMKshVtYXU == NCWuNbixTT){PWnziTyXZd = true;}
      else if(NCWuNbixTT == DMKshVtYXU){SYxEnDVhdu = true;}
      if(ocAjIQJyuN == PyaWgeulfV){CHcxLCYHXU = true;}
      if(uprSVbpyPf == rQqQRkyJmD){oPMrycPupw = true;}
      if(qXVBQFkRxs == MxcdmWdEJj){ynWHPtmATX = true;}
      while(PyaWgeulfV == ocAjIQJyuN){PDpFXTcuIq = true;}
      while(rQqQRkyJmD == rQqQRkyJmD){IYsQfXYafw = true;}
      while(MxcdmWdEJj == MxcdmWdEJj){MhfgbImUyR = true;}
      if(YbWmmlTWJj == true){YbWmmlTWJj = false;}
      if(LaTWtxLFcc == true){LaTWtxLFcc = false;}
      if(cYKQGSVwiJ == true){cYKQGSVwiJ = false;}
      if(AqbwwPtDcf == true){AqbwwPtDcf = false;}
      if(RaACrlcFjp == true){RaACrlcFjp = false;}
      if(LXbgGARFfU == true){LXbgGARFfU = false;}
      if(PWnziTyXZd == true){PWnziTyXZd = false;}
      if(CHcxLCYHXU == true){CHcxLCYHXU = false;}
      if(oPMrycPupw == true){oPMrycPupw = false;}
      if(ynWHPtmATX == true){ynWHPtmATX = false;}
      if(mEfFmDtdVD == true){mEfFmDtdVD = false;}
      if(bJhLfzJuBG == true){bJhLfzJuBG = false;}
      if(EChdfOkVgQ == true){EChdfOkVgQ = false;}
      if(PXnzgjjGFu == true){PXnzgjjGFu = false;}
      if(hkZlUnFLbP == true){hkZlUnFLbP = false;}
      if(jqzMTkWBZY == true){jqzMTkWBZY = false;}
      if(SYxEnDVhdu == true){SYxEnDVhdu = false;}
      if(PDpFXTcuIq == true){PDpFXTcuIq = false;}
      if(IYsQfXYafw == true){IYsQfXYafw = false;}
      if(MhfgbImUyR == true){MhfgbImUyR = false;}
    } 
}; 

#include <stdio.h>
#include <string>
#include <iostream>

using namespace std;

class JSVEGYBCEP
{ 
  void hQczHgYbXm()
  { 
      bool yWcohojYXC = false;
      bool OjwWeOIGgG = false;
      bool DZcKxnnKKn = false;
      bool yRjJWqCVSb = false;
      bool lWDHkzghue = false;
      bool zFFKpXtRqt = false;
      bool AZOjDwAlzm = false;
      bool MlnEUfVVZy = false;
      bool GGOenrMYSy = false;
      bool wkdYaLISDn = false;
      bool zuhHSMRBoK = false;
      bool CXrsmtKiuL = false;
      bool jaTAqbHOcI = false;
      bool XCSkzLYGMb = false;
      bool aKjIwYrkkg = false;
      bool BZXrEYbgQq = false;
      bool ECXflzFGtb = false;
      bool UDVTjYpKER = false;
      bool iWcuHyeagZ = false;
      bool jqpVXYmDWx = false;
      string sVWfnJieAs;
      string nnBGubRdQw;
      string YUkEUmmkqD;
      string niLsqfkeXS;
      string WKVxtnmMbH;
      string AIHYgYjSLA;
      string WymxEFcHPP;
      string pmFyZNznKf;
      string zaIjCfblMS;
      string uSTWwYXYro;
      string LfwMQNCIzS;
      string YwIeAwoDtY;
      string cqtwTXShBA;
      string pZuVUTdwSo;
      string cSlsszJnVA;
      string iUdTLTkzrr;
      string tUHzczBmKg;
      string RsjFuJFBDU;
      string NeLBRVuaJA;
      string idEZtHLmQt;
      if(sVWfnJieAs == LfwMQNCIzS){yWcohojYXC = true;}
      else if(LfwMQNCIzS == sVWfnJieAs){zuhHSMRBoK = true;}
      if(nnBGubRdQw == YwIeAwoDtY){OjwWeOIGgG = true;}
      else if(YwIeAwoDtY == nnBGubRdQw){CXrsmtKiuL = true;}
      if(YUkEUmmkqD == cqtwTXShBA){DZcKxnnKKn = true;}
      else if(cqtwTXShBA == YUkEUmmkqD){jaTAqbHOcI = true;}
      if(niLsqfkeXS == pZuVUTdwSo){yRjJWqCVSb = true;}
      else if(pZuVUTdwSo == niLsqfkeXS){XCSkzLYGMb = true;}
      if(WKVxtnmMbH == cSlsszJnVA){lWDHkzghue = true;}
      else if(cSlsszJnVA == WKVxtnmMbH){aKjIwYrkkg = true;}
      if(AIHYgYjSLA == iUdTLTkzrr){zFFKpXtRqt = true;}
      else if(iUdTLTkzrr == AIHYgYjSLA){BZXrEYbgQq = true;}
      if(WymxEFcHPP == tUHzczBmKg){AZOjDwAlzm = true;}
      else if(tUHzczBmKg == WymxEFcHPP){ECXflzFGtb = true;}
      if(pmFyZNznKf == RsjFuJFBDU){MlnEUfVVZy = true;}
      if(zaIjCfblMS == NeLBRVuaJA){GGOenrMYSy = true;}
      if(uSTWwYXYro == idEZtHLmQt){wkdYaLISDn = true;}
      while(RsjFuJFBDU == pmFyZNznKf){UDVTjYpKER = true;}
      while(NeLBRVuaJA == NeLBRVuaJA){iWcuHyeagZ = true;}
      while(idEZtHLmQt == idEZtHLmQt){jqpVXYmDWx = true;}
      if(yWcohojYXC == true){yWcohojYXC = false;}
      if(OjwWeOIGgG == true){OjwWeOIGgG = false;}
      if(DZcKxnnKKn == true){DZcKxnnKKn = false;}
      if(yRjJWqCVSb == true){yRjJWqCVSb = false;}
      if(lWDHkzghue == true){lWDHkzghue = false;}
      if(zFFKpXtRqt == true){zFFKpXtRqt = false;}
      if(AZOjDwAlzm == true){AZOjDwAlzm = false;}
      if(MlnEUfVVZy == true){MlnEUfVVZy = false;}
      if(GGOenrMYSy == true){GGOenrMYSy = false;}
      if(wkdYaLISDn == true){wkdYaLISDn = false;}
      if(zuhHSMRBoK == true){zuhHSMRBoK = false;}
      if(CXrsmtKiuL == true){CXrsmtKiuL = false;}
      if(jaTAqbHOcI == true){jaTAqbHOcI = false;}
      if(XCSkzLYGMb == true){XCSkzLYGMb = false;}
      if(aKjIwYrkkg == true){aKjIwYrkkg = false;}
      if(BZXrEYbgQq == true){BZXrEYbgQq = false;}
      if(ECXflzFGtb == true){ECXflzFGtb = false;}
      if(UDVTjYpKER == true){UDVTjYpKER = false;}
      if(iWcuHyeagZ == true){iWcuHyeagZ = false;}
      if(jqpVXYmDWx == true){jqpVXYmDWx = false;}
    } 
}; 

#include <stdio.h>
#include <string>
#include <iostream>

using namespace std;

class XIHTDLFAFM
{ 
  void mxNBlmxaMW()
  { 
      bool oCmEGfpZUF = false;
      bool aQHBRiAosB = false;
      bool ASDxmBbhVC = false;
      bool ylzNccsfTU = false;
      bool dGrNXzROrJ = false;
      bool dmphqWyoQE = false;
      bool DIESGoRtYo = false;
      bool pTVYWtZoZm = false;
      bool jMoMYLuQVG = false;
      bool dEPOQVBZGi = false;
      bool hWpCTJQRKx = false;
      bool wIPcUpSrtu = false;
      bool sAYSLkEGIA = false;
      bool rAhqFMWmrO = false;
      bool tdFXJUnltZ = false;
      bool DJfLohyWqi = false;
      bool EJJKFwZYXH = false;
      bool TfzeAPtAJp = false;
      bool wRMhxhUWBX = false;
      bool NzuJJjUuSM = false;
      string RDHipSlQrh;
      string duHbfQQbsG;
      string aYwksOaLIX;
      string ssunUYRTEn;
      string dpkjkyIxQz;
      string eAcxJrHWoa;
      string rZITnjzTGG;
      string ywWEcjnLAu;
      string FduIsrQyec;
      string RuedwDhQwA;
      string yoqJBidXTY;
      string EuVtUOezau;
      string TAodoDRsIZ;
      string CWOzwQlkHK;
      string OrdFYVYjIh;
      string YhpBgxAMUx;
      string QkPtVgfWxE;
      string bsrPfaWiFS;
      string egjZAUgNSz;
      string QZQKsdcjir;
      if(RDHipSlQrh == yoqJBidXTY){oCmEGfpZUF = true;}
      else if(yoqJBidXTY == RDHipSlQrh){hWpCTJQRKx = true;}
      if(duHbfQQbsG == EuVtUOezau){aQHBRiAosB = true;}
      else if(EuVtUOezau == duHbfQQbsG){wIPcUpSrtu = true;}
      if(aYwksOaLIX == TAodoDRsIZ){ASDxmBbhVC = true;}
      else if(TAodoDRsIZ == aYwksOaLIX){sAYSLkEGIA = true;}
      if(ssunUYRTEn == CWOzwQlkHK){ylzNccsfTU = true;}
      else if(CWOzwQlkHK == ssunUYRTEn){rAhqFMWmrO = true;}
      if(dpkjkyIxQz == OrdFYVYjIh){dGrNXzROrJ = true;}
      else if(OrdFYVYjIh == dpkjkyIxQz){tdFXJUnltZ = true;}
      if(eAcxJrHWoa == YhpBgxAMUx){dmphqWyoQE = true;}
      else if(YhpBgxAMUx == eAcxJrHWoa){DJfLohyWqi = true;}
      if(rZITnjzTGG == QkPtVgfWxE){DIESGoRtYo = true;}
      else if(QkPtVgfWxE == rZITnjzTGG){EJJKFwZYXH = true;}
      if(ywWEcjnLAu == bsrPfaWiFS){pTVYWtZoZm = true;}
      if(FduIsrQyec == egjZAUgNSz){jMoMYLuQVG = true;}
      if(RuedwDhQwA == QZQKsdcjir){dEPOQVBZGi = true;}
      while(bsrPfaWiFS == ywWEcjnLAu){TfzeAPtAJp = true;}
      while(egjZAUgNSz == egjZAUgNSz){wRMhxhUWBX = true;}
      while(QZQKsdcjir == QZQKsdcjir){NzuJJjUuSM = true;}
      if(oCmEGfpZUF == true){oCmEGfpZUF = false;}
      if(aQHBRiAosB == true){aQHBRiAosB = false;}
      if(ASDxmBbhVC == true){ASDxmBbhVC = false;}
      if(ylzNccsfTU == true){ylzNccsfTU = false;}
      if(dGrNXzROrJ == true){dGrNXzROrJ = false;}
      if(dmphqWyoQE == true){dmphqWyoQE = false;}
      if(DIESGoRtYo == true){DIESGoRtYo = false;}
      if(pTVYWtZoZm == true){pTVYWtZoZm = false;}
      if(jMoMYLuQVG == true){jMoMYLuQVG = false;}
      if(dEPOQVBZGi == true){dEPOQVBZGi = false;}
      if(hWpCTJQRKx == true){hWpCTJQRKx = false;}
      if(wIPcUpSrtu == true){wIPcUpSrtu = false;}
      if(sAYSLkEGIA == true){sAYSLkEGIA = false;}
      if(rAhqFMWmrO == true){rAhqFMWmrO = false;}
      if(tdFXJUnltZ == true){tdFXJUnltZ = false;}
      if(DJfLohyWqi == true){DJfLohyWqi = false;}
      if(EJJKFwZYXH == true){EJJKFwZYXH = false;}
      if(TfzeAPtAJp == true){TfzeAPtAJp = false;}
      if(wRMhxhUWBX == true){wRMhxhUWBX = false;}
      if(NzuJJjUuSM == true){NzuJJjUuSM = false;}
    } 
}; 

#include <stdio.h>
#include <string>
#include <iostream>

using namespace std;

class UPPTXQPATS
{ 
  void zjJbWrygQY()
  { 
      bool SUgjHPScTw = false;
      bool fOyjVVXMae = false;
      bool sJyPzBRTsu = false;
      bool IxgKtFYbyx = false;
      bool EBMxTGWllM = false;
      bool rinuyJVQjF = false;
      bool yyiYmihnpP = false;
      bool xuNdaqwBws = false;
      bool eeggCZIlLk = false;
      bool jHrszFSnJk = false;
      bool YhwWPxVKTG = false;
      bool NTUpaSQCVN = false;
      bool ZubFfoDUdV = false;
      bool YHrLUwhKRF = false;
      bool EyRFWQmBRf = false;
      bool nPRuLcGqqZ = false;
      bool sAfyhFbIVd = false;
      bool jGjQJadWhO = false;
      bool CBCygIowzr = false;
      bool oqTrLDSTXR = false;
      string ZJVpmgshzw;
      string FaozyuUFKx;
      string ppFRhfzsMZ;
      string jXzJIPFMXE;
      string OxPCCijKDY;
      string liHoNiEEHG;
      string hAUYMhaeqh;
      string oHhGhsaCPz;
      string uopnnBPKwY;
      string LBEQWObppT;
      string HESUidBBHT;
      string ICnHLrJgkL;
      string SPiRWmCQiS;
      string RjLuqpVUEJ;
      string ANDbPJNxEd;
      string EuFeBrVAyw;
      string ajRnCXpsPU;
      string WobbLELGCN;
      string ZFMFNWegtz;
      string UUWIhPuOZq;
      if(ZJVpmgshzw == HESUidBBHT){SUgjHPScTw = true;}
      else if(HESUidBBHT == ZJVpmgshzw){YhwWPxVKTG = true;}
      if(FaozyuUFKx == ICnHLrJgkL){fOyjVVXMae = true;}
      else if(ICnHLrJgkL == FaozyuUFKx){NTUpaSQCVN = true;}
      if(ppFRhfzsMZ == SPiRWmCQiS){sJyPzBRTsu = true;}
      else if(SPiRWmCQiS == ppFRhfzsMZ){ZubFfoDUdV = true;}
      if(jXzJIPFMXE == RjLuqpVUEJ){IxgKtFYbyx = true;}
      else if(RjLuqpVUEJ == jXzJIPFMXE){YHrLUwhKRF = true;}
      if(OxPCCijKDY == ANDbPJNxEd){EBMxTGWllM = true;}
      else if(ANDbPJNxEd == OxPCCijKDY){EyRFWQmBRf = true;}
      if(liHoNiEEHG == EuFeBrVAyw){rinuyJVQjF = true;}
      else if(EuFeBrVAyw == liHoNiEEHG){nPRuLcGqqZ = true;}
      if(hAUYMhaeqh == ajRnCXpsPU){yyiYmihnpP = true;}
      else if(ajRnCXpsPU == hAUYMhaeqh){sAfyhFbIVd = true;}
      if(oHhGhsaCPz == WobbLELGCN){xuNdaqwBws = true;}
      if(uopnnBPKwY == ZFMFNWegtz){eeggCZIlLk = true;}
      if(LBEQWObppT == UUWIhPuOZq){jHrszFSnJk = true;}
      while(WobbLELGCN == oHhGhsaCPz){jGjQJadWhO = true;}
      while(ZFMFNWegtz == ZFMFNWegtz){CBCygIowzr = true;}
      while(UUWIhPuOZq == UUWIhPuOZq){oqTrLDSTXR = true;}
      if(SUgjHPScTw == true){SUgjHPScTw = false;}
      if(fOyjVVXMae == true){fOyjVVXMae = false;}
      if(sJyPzBRTsu == true){sJyPzBRTsu = false;}
      if(IxgKtFYbyx == true){IxgKtFYbyx = false;}
      if(EBMxTGWllM == true){EBMxTGWllM = false;}
      if(rinuyJVQjF == true){rinuyJVQjF = false;}
      if(yyiYmihnpP == true){yyiYmihnpP = false;}
      if(xuNdaqwBws == true){xuNdaqwBws = false;}
      if(eeggCZIlLk == true){eeggCZIlLk = false;}
      if(jHrszFSnJk == true){jHrszFSnJk = false;}
      if(YhwWPxVKTG == true){YhwWPxVKTG = false;}
      if(NTUpaSQCVN == true){NTUpaSQCVN = false;}
      if(ZubFfoDUdV == true){ZubFfoDUdV = false;}
      if(YHrLUwhKRF == true){YHrLUwhKRF = false;}
      if(EyRFWQmBRf == true){EyRFWQmBRf = false;}
      if(nPRuLcGqqZ == true){nPRuLcGqqZ = false;}
      if(sAfyhFbIVd == true){sAfyhFbIVd = false;}
      if(jGjQJadWhO == true){jGjQJadWhO = false;}
      if(CBCygIowzr == true){CBCygIowzr = false;}
      if(oqTrLDSTXR == true){oqTrLDSTXR = false;}
    } 
}; 

#include <stdio.h>
#include <string>
#include <iostream>

using namespace std;

class ZNAVFYUVON
{ 
  void MbloEubIgB()
  { 
      bool nKhokWSBNW = false;
      bool JfbTYWwzLw = false;
      bool WLPbgFDprE = false;
      bool chZiUsmRWq = false;
      bool rUmlZzmJEP = false;
      bool cwWmJbuFHe = false;
      bool SbJJsbFrzf = false;
      bool jdcUECYcBh = false;
      bool StlbbfKmZr = false;
      bool ygJqXQKcry = false;
      bool suKYfyTChH = false;
      bool DREYMXphhN = false;
      bool QSSGhCTkwn = false;
      bool qukCoXjNuc = false;
      bool LIERofcjez = false;
      bool acuehabDqd = false;
      bool diFtQHgBjn = false;
      bool ncmEBHNARB = false;
      bool ZiGwNOAFUO = false;
      bool ufVQycDJBh = false;
      string XcISnDUtan;
      string rWLYRUjTwb;
      string dhgCjxlqrd;
      string tqyOMBSBaA;
      string TQnqZYmuBS;
      string pFRzSpuGiq;
      string RcBxtBYxMP;
      string BhrwIMspcL;
      string KppYmoYzwV;
      string GMSihpOqxX;
      string AoikRHMVqF;
      string wFcXgblbKF;
      string cYApJnPJNR;
      string yCrELUGFHe;
      string bDIqZAjtCG;
      string Dmgquaorbd;
      string rDHYHsMYMM;
      string UgUecYQJQI;
      string EUHHNKVAIj;
      string eOcdmYwiig;
      if(XcISnDUtan == AoikRHMVqF){nKhokWSBNW = true;}
      else if(AoikRHMVqF == XcISnDUtan){suKYfyTChH = true;}
      if(rWLYRUjTwb == wFcXgblbKF){JfbTYWwzLw = true;}
      else if(wFcXgblbKF == rWLYRUjTwb){DREYMXphhN = true;}
      if(dhgCjxlqrd == cYApJnPJNR){WLPbgFDprE = true;}
      else if(cYApJnPJNR == dhgCjxlqrd){QSSGhCTkwn = true;}
      if(tqyOMBSBaA == yCrELUGFHe){chZiUsmRWq = true;}
      else if(yCrELUGFHe == tqyOMBSBaA){qukCoXjNuc = true;}
      if(TQnqZYmuBS == bDIqZAjtCG){rUmlZzmJEP = true;}
      else if(bDIqZAjtCG == TQnqZYmuBS){LIERofcjez = true;}
      if(pFRzSpuGiq == Dmgquaorbd){cwWmJbuFHe = true;}
      else if(Dmgquaorbd == pFRzSpuGiq){acuehabDqd = true;}
      if(RcBxtBYxMP == rDHYHsMYMM){SbJJsbFrzf = true;}
      else if(rDHYHsMYMM == RcBxtBYxMP){diFtQHgBjn = true;}
      if(BhrwIMspcL == UgUecYQJQI){jdcUECYcBh = true;}
      if(KppYmoYzwV == EUHHNKVAIj){StlbbfKmZr = true;}
      if(GMSihpOqxX == eOcdmYwiig){ygJqXQKcry = true;}
      while(UgUecYQJQI == BhrwIMspcL){ncmEBHNARB = true;}
      while(EUHHNKVAIj == EUHHNKVAIj){ZiGwNOAFUO = true;}
      while(eOcdmYwiig == eOcdmYwiig){ufVQycDJBh = true;}
      if(nKhokWSBNW == true){nKhokWSBNW = false;}
      if(JfbTYWwzLw == true){JfbTYWwzLw = false;}
      if(WLPbgFDprE == true){WLPbgFDprE = false;}
      if(chZiUsmRWq == true){chZiUsmRWq = false;}
      if(rUmlZzmJEP == true){rUmlZzmJEP = false;}
      if(cwWmJbuFHe == true){cwWmJbuFHe = false;}
      if(SbJJsbFrzf == true){SbJJsbFrzf = false;}
      if(jdcUECYcBh == true){jdcUECYcBh = false;}
      if(StlbbfKmZr == true){StlbbfKmZr = false;}
      if(ygJqXQKcry == true){ygJqXQKcry = false;}
      if(suKYfyTChH == true){suKYfyTChH = false;}
      if(DREYMXphhN == true){DREYMXphhN = false;}
      if(QSSGhCTkwn == true){QSSGhCTkwn = false;}
      if(qukCoXjNuc == true){qukCoXjNuc = false;}
      if(LIERofcjez == true){LIERofcjez = false;}
      if(acuehabDqd == true){acuehabDqd = false;}
      if(diFtQHgBjn == true){diFtQHgBjn = false;}
      if(ncmEBHNARB == true){ncmEBHNARB = false;}
      if(ZiGwNOAFUO == true){ZiGwNOAFUO = false;}
      if(ufVQycDJBh == true){ufVQycDJBh = false;}
    } 
}; 

#include <stdio.h>
#include <string>
#include <iostream>

using namespace std;

class TRRFKPVHRJ
{ 
  void shALziyVJa()
  { 
      bool PuWSEzmcQT = false;
      bool VQbGZugMpo = false;
      bool EzTcPfCaeB = false;
      bool wJiJZfdyVy = false;
      bool kbxzOIZfka = false;
      bool ttUSKPatDX = false;
      bool xLxOXLhSgt = false;
      bool QiprDcBUOG = false;
      bool FVoEHBnQyJ = false;
      bool EjuyBZeiaC = false;
      bool ZHzqJLDcOh = false;
      bool OFfwRaVrHV = false;
      bool WQUWogmrqd = false;
      bool xSsLfyVyuu = false;
      bool yLBPuzDyfZ = false;
      bool DmaFeJmcBb = false;
      bool XKljkuLTPT = false;
      bool EXwLXLPEbp = false;
      bool cnMQaKqngW = false;
      bool KswSXmZRdz = false;
      string llgMFTDdQe;
      string mzQNxizOMl;
      string UJIkpaYgGq;
      string bpDSudKsJV;
      string jxYBUFUShi;
      string ocrcjccuWd;
      string tWXYDxuxRT;
      string XGEpBXjGPj;
      string GGyXCyxaiS;
      string AOVfbNZAyX;
      string KWBOKhXCbM;
      string sWflXnYzVE;
      string zTOzSJkYOm;
      string yiIrZiEiMT;
      string auzWZdiiHm;
      string oZetYpICwB;
      string xymnNJNnMo;
      string iomCcEUqZo;
      string aBRayXEOTj;
      string ZCmLWbHKIP;
      if(llgMFTDdQe == KWBOKhXCbM){PuWSEzmcQT = true;}
      else if(KWBOKhXCbM == llgMFTDdQe){ZHzqJLDcOh = true;}
      if(mzQNxizOMl == sWflXnYzVE){VQbGZugMpo = true;}
      else if(sWflXnYzVE == mzQNxizOMl){OFfwRaVrHV = true;}
      if(UJIkpaYgGq == zTOzSJkYOm){EzTcPfCaeB = true;}
      else if(zTOzSJkYOm == UJIkpaYgGq){WQUWogmrqd = true;}
      if(bpDSudKsJV == yiIrZiEiMT){wJiJZfdyVy = true;}
      else if(yiIrZiEiMT == bpDSudKsJV){xSsLfyVyuu = true;}
      if(jxYBUFUShi == auzWZdiiHm){kbxzOIZfka = true;}
      else if(auzWZdiiHm == jxYBUFUShi){yLBPuzDyfZ = true;}
      if(ocrcjccuWd == oZetYpICwB){ttUSKPatDX = true;}
      else if(oZetYpICwB == ocrcjccuWd){DmaFeJmcBb = true;}
      if(tWXYDxuxRT == xymnNJNnMo){xLxOXLhSgt = true;}
      else if(xymnNJNnMo == tWXYDxuxRT){XKljkuLTPT = true;}
      if(XGEpBXjGPj == iomCcEUqZo){QiprDcBUOG = true;}
      if(GGyXCyxaiS == aBRayXEOTj){FVoEHBnQyJ = true;}
      if(AOVfbNZAyX == ZCmLWbHKIP){EjuyBZeiaC = true;}
      while(iomCcEUqZo == XGEpBXjGPj){EXwLXLPEbp = true;}
      while(aBRayXEOTj == aBRayXEOTj){cnMQaKqngW = true;}
      while(ZCmLWbHKIP == ZCmLWbHKIP){KswSXmZRdz = true;}
      if(PuWSEzmcQT == true){PuWSEzmcQT = false;}
      if(VQbGZugMpo == true){VQbGZugMpo = false;}
      if(EzTcPfCaeB == true){EzTcPfCaeB = false;}
      if(wJiJZfdyVy == true){wJiJZfdyVy = false;}
      if(kbxzOIZfka == true){kbxzOIZfka = false;}
      if(ttUSKPatDX == true){ttUSKPatDX = false;}
      if(xLxOXLhSgt == true){xLxOXLhSgt = false;}
      if(QiprDcBUOG == true){QiprDcBUOG = false;}
      if(FVoEHBnQyJ == true){FVoEHBnQyJ = false;}
      if(EjuyBZeiaC == true){EjuyBZeiaC = false;}
      if(ZHzqJLDcOh == true){ZHzqJLDcOh = false;}
      if(OFfwRaVrHV == true){OFfwRaVrHV = false;}
      if(WQUWogmrqd == true){WQUWogmrqd = false;}
      if(xSsLfyVyuu == true){xSsLfyVyuu = false;}
      if(yLBPuzDyfZ == true){yLBPuzDyfZ = false;}
      if(DmaFeJmcBb == true){DmaFeJmcBb = false;}
      if(XKljkuLTPT == true){XKljkuLTPT = false;}
      if(EXwLXLPEbp == true){EXwLXLPEbp = false;}
      if(cnMQaKqngW == true){cnMQaKqngW = false;}
      if(KswSXmZRdz == true){KswSXmZRdz = false;}
    } 
}; 

#include <stdio.h>
#include <string>
#include <iostream>

using namespace std;

class HHBLBVDZFR
{ 
  void txEKjPlKlD()
  { 
      bool hcNSDfkyET = false;
      bool FiaFsGwMpt = false;
      bool ghhAUxLoqO = false;
      bool TirDrZzZBm = false;
      bool YByTEuhfHO = false;
      bool XTCgoztNxF = false;
      bool TwGpzVqWNT = false;
      bool HWLCVfjOuZ = false;
      bool PfgbpzfLNr = false;
      bool ekbwSbtITh = false;
      bool EKSnHBIAKJ = false;
      bool PeRXonOdzA = false;
      bool peIUnVuOjs = false;
      bool kbuNVFNsUo = false;
      bool qfAEczSEHd = false;
      bool NQISrYBxzl = false;
      bool FtPRXnjEBi = false;
      bool aVJJIJeDrn = false;
      bool CpHFfUSbUw = false;
      bool DQshiwPign = false;
      string FVuOdyePPZ;
      string gyomzWueRz;
      string FnPiGLtScP;
      string VAIRMFphSd;
      string GCOmDBaFWf;
      string TLUztRGLHS;
      string EMlAEgoXZt;
      string gMLnrCZbfE;
      string kouxwKfJXu;
      string zeaAZtQOiW;
      string MCebzGAfGq;
      string qdeChPQnDt;
      string AezduClKOx;
      string lKrJrhJSyp;
      string JzCfDUzXSJ;
      string dbLoVhCBgw;
      string ToRJEglVKA;
      string GxpMjBbTZa;
      string DNrETKzDmJ;
      string eIlBVUgsaW;
      if(FVuOdyePPZ == MCebzGAfGq){hcNSDfkyET = true;}
      else if(MCebzGAfGq == FVuOdyePPZ){EKSnHBIAKJ = true;}
      if(gyomzWueRz == qdeChPQnDt){FiaFsGwMpt = true;}
      else if(qdeChPQnDt == gyomzWueRz){PeRXonOdzA = true;}
      if(FnPiGLtScP == AezduClKOx){ghhAUxLoqO = true;}
      else if(AezduClKOx == FnPiGLtScP){peIUnVuOjs = true;}
      if(VAIRMFphSd == lKrJrhJSyp){TirDrZzZBm = true;}
      else if(lKrJrhJSyp == VAIRMFphSd){kbuNVFNsUo = true;}
      if(GCOmDBaFWf == JzCfDUzXSJ){YByTEuhfHO = true;}
      else if(JzCfDUzXSJ == GCOmDBaFWf){qfAEczSEHd = true;}
      if(TLUztRGLHS == dbLoVhCBgw){XTCgoztNxF = true;}
      else if(dbLoVhCBgw == TLUztRGLHS){NQISrYBxzl = true;}
      if(EMlAEgoXZt == ToRJEglVKA){TwGpzVqWNT = true;}
      else if(ToRJEglVKA == EMlAEgoXZt){FtPRXnjEBi = true;}
      if(gMLnrCZbfE == GxpMjBbTZa){HWLCVfjOuZ = true;}
      if(kouxwKfJXu == DNrETKzDmJ){PfgbpzfLNr = true;}
      if(zeaAZtQOiW == eIlBVUgsaW){ekbwSbtITh = true;}
      while(GxpMjBbTZa == gMLnrCZbfE){aVJJIJeDrn = true;}
      while(DNrETKzDmJ == DNrETKzDmJ){CpHFfUSbUw = true;}
      while(eIlBVUgsaW == eIlBVUgsaW){DQshiwPign = true;}
      if(hcNSDfkyET == true){hcNSDfkyET = false;}
      if(FiaFsGwMpt == true){FiaFsGwMpt = false;}
      if(ghhAUxLoqO == true){ghhAUxLoqO = false;}
      if(TirDrZzZBm == true){TirDrZzZBm = false;}
      if(YByTEuhfHO == true){YByTEuhfHO = false;}
      if(XTCgoztNxF == true){XTCgoztNxF = false;}
      if(TwGpzVqWNT == true){TwGpzVqWNT = false;}
      if(HWLCVfjOuZ == true){HWLCVfjOuZ = false;}
      if(PfgbpzfLNr == true){PfgbpzfLNr = false;}
      if(ekbwSbtITh == true){ekbwSbtITh = false;}
      if(EKSnHBIAKJ == true){EKSnHBIAKJ = false;}
      if(PeRXonOdzA == true){PeRXonOdzA = false;}
      if(peIUnVuOjs == true){peIUnVuOjs = false;}
      if(kbuNVFNsUo == true){kbuNVFNsUo = false;}
      if(qfAEczSEHd == true){qfAEczSEHd = false;}
      if(NQISrYBxzl == true){NQISrYBxzl = false;}
      if(FtPRXnjEBi == true){FtPRXnjEBi = false;}
      if(aVJJIJeDrn == true){aVJJIJeDrn = false;}
      if(CpHFfUSbUw == true){CpHFfUSbUw = false;}
      if(DQshiwPign == true){DQshiwPign = false;}
    } 
}; 
