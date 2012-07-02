#ifndef _FFV_H_
#define _FFV_H_

// #################################################################
//
// FFV : Frontflow / violet
//
// Copyright (c) All right reserved. 2012
//
// Institute of Industrial Science, The University of Tokyo, Japan. 
//
// #################################################################
//
// 以下のマクロはcpm_Define.hで定義されている
//   REAL_TYPE
//   X_MINUS, Y_MINUS, Z_MINUS, X_PLUS, Y_PLUS, Z_PLUS
//   X_DIR, Y_DIR, Z_DIR
//   PLUS2MINUS, MINUS2PLUS, BOTH

/** 
 * @file ffv.h
 * @brief FFV Class Header
 * @author kero
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <iostream>
#include <fstream>

#include "cpm_ParaManager.h"
#include "cpm_TextParserDomain.h"

#include "DomainInfo.h"

#include "FB_Define.h"
#include "ffv_Define.h"
#include "mydebug.h"
#include "FBUtility.h"
#include "Control.h"
#include "Alloc.h"
#include "FileIO.h"
#include "ParseBC.h"
#include "ParseMat.h"
#include "VoxInfo.h"
#include "TPControl.h"
#include "ffv_SetBC.h"
#include "CompoFraction.h"
#include "dfi.h"

#include "omp.h"


#include "IP_Duct.h"
#include "IP_PPLT2D.h"
#include "IP_SHC1D.h"
#include "IP_PMT.h"
#include "IP_Rect.h"
#include "IP_Step.h"
#include "IP_Cylinder.h"
#include "IP_Polygon.h"
#include "IP_Sphere.h"

// FX10 profiler
#if defined __K_FPCOLL
#include "fjcoll.h"
#elif defined __FX_FAPP
#include "fj_tool/fjcoll.h"
#endif

// Performance Monitor
#include "PerfMonitor.h"

// Polylib
#include "Polylib.h"
#include "MPIPolylib.h"


using namespace std;
using namespace pm_lib;
using namespace PolylibNS;


class FFV : public DomainInfo {
private:
  int ffv_procGrp;         ///< プロセスグループ番号 => 0
  int session_maxStep;     ///< セッションのステップ数
  int session_currentStep; ///< セッションの現在のステップ
  int ModeTiming;          ///< タイミング測定管理フラグ
  
  unsigned long G_Acell;   ///< グローバルなActive cell
  unsigned long G_Fcell;   ///< グローバルなFluid cell
  unsigned long G_Wcell;   ///< グローバルなSolid cell
  
  unsigned long L_Acell;   ///< ローカルなActive cell
  unsigned long L_Fcell;   ///< ローカルなFluid cell
  unsigned long L_Wcell;   ///< ローカルなSolid cell
  
  // Fortranへの引数
  REAL_TYPE *dh;    ///< 格子幅（無次元）
  REAL_TYPE *dh0;   ///< 格子幅（有次元）
  REAL_TYPE v00[4]; ///< 参照速度
  
  
  // データ領域ポインタ
  
  // Vector3D
  REAL_TYPE *d_v;
  REAL_TYPE *d_vc;
  REAL_TYPE *d_v0;
  REAL_TYPE *d_wv;
  REAL_TYPE *d_abf;
  REAL_TYPE *d_vf0;
  REAL_TYPE *d_av;
  REAL_TYPE *d_wvex;
  REAL_TYPE *d_qbc;
  
  // Scalar3D
  int *d_mid;
  int *d_bcd;
  int *d_bcp;
  int *d_bcv;
  int *d_bh1;
  int *d_bh2;
  REAL_TYPE  *d_ws;
  REAL_TYPE  *d_p;
  REAL_TYPE  *d_wk2;
  REAL_TYPE  *d_dp;
  REAL_TYPE  *d_p0;
  REAL_TYPE  *d_t;
  REAL_TYPE  *d_t0;
  REAL_TYPE  *d_vt;
  REAL_TYPE  *d_vof;
  REAL_TYPE  *d_ap;
  REAL_TYPE  *d_at;
  float      *d_cvf;
  
  // Coarse initial
  REAL_TYPE *d_r_v;  ///< 粗格子の速度
  REAL_TYPE *d_r_p;  ///< 粗格子の圧力
  REAL_TYPE *d_r_t;  ///< 粗格子の温度
  
  REAL_TYPE** component_array; ///< コンポーネントワーク配列のアドレス管理
  
  int* compo_global_bbox; ///< グローバルなコンポーネントBbox 表示に利用
  
  
  // カット
  REAL_TYPE  *d_cut; ///< 距離情報
  int        *d_bid; ///< BC
  
  
  FILE *fp_b;  ///< 基本情報
  FILE *fp_w;  ///< 壁面情報
  FILE *fp_c;  ///< コンポーネント情報
  FILE *fp_d;  ///< 流量収支情報
  FILE *fp_i;  ///< 反復履歴情報
  FILE *fp_f;  ///< 力の履歴情報
  
  Control C;                 ///< 制御パラメータクラス
  FileIO F;                  ///< ファイル入出力クラス
  DTcntl DT;                 ///< 時間制御クラス
  ParseMat M;                ///< 媒質パラメータ管理クラス
  Intrinsic* Ex;             ///< pointer to a base class
  ItrCtl IC[ItrCtl::ic_END]; ///< 反復情報管理クラス
  ReferenceFrame RF;         ///< 参照座標系クラス
  MediumList* mat;           ///< 媒質リスト
  CompoList* cmp;            ///< コンポーネントリスト
  PerfMonitor PM;            ///< 性能モニタクラス
  VoxInfo V;                 ///< ボクセル前処理クラス
  ParseBC B;                 ///< 境界条件のパースクラス
  TPControl tpCntl;          ///< テキストパーサのラッパークラス
  SetBC3D BC;                ///< BCクラス
  DFI DFI;                   ///< 分散ファイルインデクス管理クラス
  // Polylib
  MPIPolylib* PL;            ///< Polylibクラス
  POLYLIB_STAT poly_stat;    ///< Polylibの戻り値
  
  char tm_label_ptr[tm_END][TM_LABEL_MAX];  ///< プロファイラ用のラベル
  
public:
  /** コンストラクタ */
  FFV();
  
  /**　デストラクタ */
  ~FFV();
  
  
private:

  /**
   * @brief Adams-Bashforth法に用いる配列のアロケーション
   * @param [in/out] total ソルバーに使用するメモリ量
   */
  void allocArray_AB2 (double &total);
  
  
  /**
   * @brief 平均値処理に用いる配列のアロケーション
   * @param [in/out] total  ソルバーに使用するメモリ量
   */
  void allocArray_Average (double &total);
  
  
  /**
   * @brief 粗格子読み込みに用いる配列のアロケーション
   * @param [in]     r_size  粗格子の領域サイズ
   * @param [in/out] prep    前処理に使用するメモリ量
   */
  void allocArray_CoarseMesh(const int* r_size, double &prep);
  
  
  /**
   * @brief コンポーネント体積率の配列のアロケーション
   * @param [in/out] prep  前処理に使用するメモリ量
   * @param [in/out] total ソルバーに使用するメモリ量
   */
  void allocArray_CompoVF(double &prep, double &total);
  
  
  /**
   * @brief カット情報の配列
   * @param [in/out] total ソルバーに使用するメモリ量
   */
  void allocArray_Cut(double &total);
  
  
  /**
   @brief コンポーネントのワーク用配列のアロケート
   @param [in/out] m_prep  前処理用のメモリサイズ
   @param [in/out] m_total 本計算用のメモリリサイズ
   @param [in]     fp      ファイルポインタ
   */
  void allocArray_Forcing(double& m_prep, double& m_total, FILE* fp);
  
  
  /**
   * @brief 熱の主計算部分に用いる配列のアロケーション
   * @param [in/out] total ソルバーに使用するメモリ量
   */
  void allocArray_Heat(double &total);
  
  
  /**
   * @brief 体積率の配列のアロケーション
   * @param [in/out] total ソルバーに使用するメモリ量
   */
  void allocArray_Interface(double &total);
  
  
  /**
   * @brief LES計算に用いる配列のアロケーション
   * @param [in/out] total ソルバーに使用するメモリ量
   */
  void allocArray_LES(double &total);
  
  
  /**
   * @brief 主計算部分に用いる配列のアロケーション
   * @param [in/out] total ソルバーに使用するメモリ量
   */
  void allocArray_Main(double &total);
  
  
  /**
   * @brief 前処理に用いる配列のアロケーション
   * @param [in/out] prep  前処理に使用するメモリ量
   * @param [in/out] total ソルバーに使用するメモリ量
   */
  void allocArray_Prep(double &prep, double &total);
  
  
  /**
   * @brief Runge-Kutta法に用いる配列のアロケーション
   * @param [in/out] total ソルバーに使用するメモリ量
   */
  void allocArray_RK(double &total);
  
  
  /**
   @brief 全Voxelモデルの媒質数とKOSの整合性をチェック
   @retval エラーコード
   */
  bool chkMediumConsistency();
  
  
  /**
   * @brief 組み込み例題のインスタンス
   * @param [in] Cref Controlクラスのポインタ
   */
  void connectExample(Control* Cref);
  
  
  /** 計算領域情報を設定する
   * @param [in] dom_file  ドメインファイル名
   */
  void DomainInitialize(const string dom_file);
  
  
  /**
   @brief 初期インデクスの情報を元に，一層拡大したインデクス値を返す
   @param [in/out] m_st 拡大された開始点（Fortranインデクス）
   @param [in/out] m_ed 拡大された終了点（Fortranインデクス）
   @param [in]     st_i 開始点（Cインデクス）
   @param [in]     len  コンポーネントの存在長さ
   @param [in]     m_x  軸方向のサイズ
   @param [in]     dir  方向
   @param m_id キーID
   */
  void EnlargeIndex(int& m_st, int& m_ed, const int st_i, const int len, const int m_x, const int dir, const int m_id);
  
  
  /**
   * @brief 固定パラメータの設定
   */
  void fixed_parameters();
  
  
  /**
   * @brief 並列処理時の各ノードの分割数を集めてファイルに保存する
   */
  void gather_DomainInfo();
  
  
  /** グローバルな領域情報を取得 
   * @return 分割指示 (1-with / 2-without)
   */
  int get_DomainInfo();
  
  
  /**
   * @brief 組み込み例題の設定
   * @param [in] Cref    コントロールクラス
   * @param [in] tpCntl  テキストパーサーのラッパー
   */
  void getExample(Control* Cref, TPControl* tpCntl);
  
  
  /**
   * @brief プロファイラのラベル取り出し
   * @param [in] key 格納番号
   * @return ラベル
   */
  inline const char* get_tm_label(const int key) 
  {
    return (const char*)tm_label_ptr[key];
  }
  
  
  /** 1ステップのコアの処理
   * @param [in] m_step   現在のステップ数
   */
  int Loop(int m_step);
  
  
  /**
   * @brief 読み込んだ領域情報のデバッグライト
   */
  void printDomainInfo();

  
  /**
   * @brief コンポーネントリストに登録されたセル要素BCのBV情報をリサイズする
   * @param [in] st 開始インデクス
   * @param [in] ed 終了インデクス
   * @param [in] n  CompoListのエントリ
   * @param [in] bx BCindex
   */
  void resizeBVface(const int* st, const int* ed, const int n, const int* bx);
  
  
  /**
   * @brief コンポーネントリストに登録されたセル要素BCのBV情報をリサイズする
   * @param [in] st 開始インデクス
   * @param [in] ed 終了インデクス
   * @param [in] n  CompoListのエントリ
   * @param [in] bx BCindex
   */
  void resizeBVcell(const int* st, const int* ed, const int n, const int* bx);
  
  
  /**
   * @brief コンポーネントリストに登録されたBV情報をリサイズする
   * @param kos KOS
   * @param isHeat 熱問題のときtrue
   */
  void resizeCompoBV(const int kos, const bool isHeat);
  
  
  /**
   * @brief 外部境界条件を読み込み，Controlクラスに保持する
   */
  void setBCinfo();
  
  
  /**
   * @brief HEX,FANコンポーネントなどの体積率とbboxなどをセット
   */
  void setComponentVF();
  
  
  
  /**
   * @brief 並列分散時のファイル名の管理を行う
   */
  void setDFI();
  
  
  /**
   * @brief コンポーネントが存在するかを保持しておく
   */
  void setEnsComponent();
  
  
  /**
   * @brief コンポーネントのローカルなBbox情報からグローバルなBbox情報を求める
   */
  void setGlobalCmpIdx();
  
  
  /**
   * @brief midの情報から各BCコンポーネントのローカルなインデクスを取得する
   */
  void setLocalCmpIdx_Binary();
  
  
  /** ParseMatクラスをセットアップし，媒質情報を入力ファイルから読み込み，媒質リストを作成する
   * @param [in] fp  ファイルポインタ
   */
  void setMediumList(FILE* fp);
  
  
  /**
   * @brief 並列化と分割の方法を保持
   * @return 並列モード
   */
  string setParallelism();
  
  
  /**
   * @brief タイミング測定区間にラベルを与えるラッパー
   * @param [in] key       キー番号
   * @param [in] label     ラベル
   * @param [in] type      測定対象タイプ(COMM or CALC)
   * @param [in] exclusive 排他測定フラグ(ディフォルトtrue)
   */
  void set_label(const int key, char* label, PerfMonitor::Type type, bool exclusive=true);
  
  
  /** モデルをセットアップ
   * @param [in] PrepMemory  前処理に必要なメモリ
   * @param [in] TotalMemory ソルバー実行に必要なメモリ
   * @param [in] fp          ファイルポインタ
   */
  void setModel(double& PrepMemory, double& TotalMemory, FILE* fp);
  
  
  /**
   * @brief タイミング測定区間にラベルを与える
   */
  void set_timing_label();
  
  
  /**
   @brief IP用にカット領域をアロケートする
   @param [in] m_prep  前処理用のメモリサイズ
   @param [in] m_total 本計算用のメモリリサイズ
   @param [in] fp      ファイルポインタ
   */
  void setup_CutInfo4IP(double& m_prep, double& m_total, FILE* fp);
  
  
  /** 毎ステップ後に行う処理 */
  bool stepPost();
  
  
  
  /**
   * @brief タイミング測定開始
   * @param [in] key 格納番号
   */
  inline void TIMING_start(const int key) 
  {
    // Intrinsic profiler
    TIMING__ PM.start((unsigned)key);
    
    // Venus FX profiler
#if defined __K_FPCOLL
    start_collection( get_tm_label(key) );
#elif defined __FX_FAPP
    fapp_start( get_tm_label(key), 0, 0);
#endif
  }
  
  
  /**
   * @brief タイミング測定終了
   * @param [in] key             格納番号
   * @param [in] flopPerTask    「タスク」あたりの計算量/通信量(バイト) (ディフォルト0)
   * @param [in] iterationCount  実行「タスク」数 (ディフォルト1)
   */
  inline void TIMING_stop(const int key, double flopPerTask=0.0, int iterationCount=1) 
  {
    // Venus FX profiler
#if defined __K_FPCOLL
    stop_collection( get_tm_label(key) );
#elif defined __FX_FAPP
    fapp_stop( get_tm_label(key), 0, 0);
#endif
    
    // Intrinsic profiler
    TIMING__ PM.stop((unsigned)key, flopPerTask, (unsigned)iterationCount);
  }
    
  
  /** コマンドラインヘルプ */
  void Usage();
  
  /**
   * @brief ポリゴンのカット情報からVBCのboxをセット
   */
  void VIBC_Bbox_from_Cut();
  
  
  /**
   * @brief BCIndexにビット情報をエンコードする
   */
  void VoxEncode();
  
  
  /**
   * @brief ボクセルをスキャンし情報を表示する
   * @param [in] fp ファイルポインタ 
   */
  void VoxScan(FILE* fp);

  
  
  
public:
  
  /**
   * @brief CPMのポインタをコピーし、ランク情報を設定
   * @param [in] m_paraMngr  cpm_ParaManagerクラス
   * @return  エラーコード
   */
  bool importCPM(cpm_ParaManager* m_paraMngr)
  {
    if ( !m_paraMngr ) return false;
    paraMngr = m_paraMngr;
    
    setDomainInfo(paraMngr, procGrp);
    
    return true;
  }
  
  
  /** 初期化 
   * 格子生成、ビットフラグ処理ほか
   * @param [in] argc  main関数の引数の個数
   * @param [in] argv  main関数の引数リスト
   */
  int Initialize(int argc, char **argv);
  
  
  /** 
   * @brief マスターノードのみ trueを返す
   * @return true(Rank==0) / false(Rank!=0)
   */
  bool IsMaster() const
  {
    return ( paraMngr->GetMyRankID() == 0 ) ? true : false;
  }
  
  
  /** シミュレーションの1ステップの処理
   *  Loop() + stepPost()
   */
  int MainLoop();
  
  
  /** シミュレーションの終了時の処理
   * プロファイルの統計処理ほか
   */
  bool Post();
  
};

#endif // _FFV_H_