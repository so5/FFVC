#ifndef _FFV_GEOM_H_
#define _FFV_GEOM_H_
//##################################################################################
//
// FFV-C : Frontflow / violet Cartesian
//
// Copyright (c) 2007-2011 VCAD System Research Program, RIKEN.
// All rights reserved.
//
// Copyright (c) 2011-2014 Institute of Industrial Science, The University of Tokyo.
// All rights reserved.
//
// Copyright (c) 2012-2014 Advanced Institute for Computational Science, RIKEN.
// All rights reserved.
//
//##################################################################################

/**
 * @file   Geometry.h
 * @brief  Geometry Class header
 * @author aics
 */

#include "DomainInfo.h"
#include "FB_Define.h"
#include "Medium.h"
#include "PolyProperty.h"
#include "Component.h"
#include "TextParser.h"

#include "Polylib.h"
#include "MPIPolylib.h"


using namespace std;
using namespace PolylibNS;
using namespace Vec3class;

class Geometry : public DomainInfo {
  
private:
  int NumSuvDiv;       ///< 再分割数
  int FillSeedDir;     ///< フィルのヒント {x_minux | x_plus |...}
  string FillMedium;   ///< フィルに使う媒質 -> int FillID
  string SeedMedium;   ///< ヒントに使う媒質 -> int SeedID
  
  
public:
  int FillID;          ///< フィル媒質ID
  int SeedID;          ///< フィルシード媒質ID
  int FillSuppress[3]; ///< PeriodicとSymmetricの外部境界面フィル抑制
  
  
public:
  
  /** コンストラクタ */
  Geometry() {

    FillID = -1;
    SeedID = -1;
    NumSuvDiv = 0;
    FillSeedDir = -1;
    
    for (int i=0; i<3; i++) {
      FillSuppress[i] = ON; // default is "fill"
    }
  }
  
  /**　デストラクタ */
  ~Geometry() {}
  

private:
  
  // d_mid[]がtargetであるセルに対して、d_pvf[]に指定値valueを代入する
  unsigned long assignVF(const int target,
                         const REAL_TYPE value,
                         const int* d_mid,
                         REAL_TYPE* d_pvf);
  
  
  // mid[]内にあるm_idのセルを数える
  unsigned long countCellM(const int* mid,
                           const int m_id,
                           const bool painted=true,
                           const int* Dsize=NULL);
  
  
  // 未ペイントセルをtargetでフィル
  unsigned long fillByID(int* mid,
                         const int target,
                         const int* Dsize=NULL);
  
  
  // 流体媒質のフィルをbid情報を元に実行
  unsigned long fillByMid(int* mid,
                          const int tgt_id,
                          const int* Dsize=NULL);
  
  
  // 未ペイントセルを周囲のbidの固体最頻値でフィル
  unsigned long fillByModalSolid(int* bcd,
                                 const int fluid_id,
                                 const int* bid,
                                 const int m_NoCompo);
  
  
  // 未ペイントセルを周囲のmidの固体最頻値でフィル
  unsigned long fillByModalSolid(int* mid, const int fluid_id, const int m_NoCompo);
  
  
  // サブセルのSolid部分の値を代入
  unsigned long fillSubCellSolid(int* smd, REAL_TYPE* svf);
  
  
  // サブセルの未ペイントセルを周囲の媒質IDの固体最頻値でフィル
  unsigned long fillSubCellByModalSolid(int* smd,
                                        const int m_NoCompo,
                                        REAL_TYPE* svf,
                                        const MediumList* mat);
  
  
  // シード点をmid[]にペイントする
  unsigned long fillSeedMid(int* mid,
                            const int face,
                            const int target,
                            const int* Dsize=NULL);
  
  
  // 点pの属するセルインデクスを求める
  // @param [in]  pt 無次元座標
  // @param [out] w  インデクス
  inline void findIndex(const Vec3r pt, int* w) const
  {
    REAL_TYPE p[3], q[3];
    p[0] = (REAL_TYPE)pt.x;
    p[1] = (REAL_TYPE)pt.y;
    p[2] = (REAL_TYPE)pt.z;
    
    q[0] = (p[0]-origin[0])/pitch[0];
    q[1] = (p[1]-origin[1])/pitch[1];
    q[2] = (p[2]-origin[2])/pitch[2];
    
    w[0] = (int)ceil(q[0]);
    w[1] = (int)ceil(q[1]);
    w[2] = (int)ceil(q[2]);
  }
  
  
  // list[]内の最頻値IDを求める
  int find_mode(const int m_sz,
                const int* list,
                const int m_NoCompo);
  
  
  // サブセル内の最頻値IDを求める
  int find_mode_smd(const int* smd, const int m_NoCompo);
  
  
  // セルに含まれるポリゴンを探索し、d_midに記録
  unsigned long findPolygonInCell(int* d_mid,
                                  MPIPolylib* PL,
                                  PolygonProperty* PG,
                                  const int m_NoCompo);
  
  
  // フィルパラメータを取得
  void getFillParam();
  
  
  /**
   * @brief インデックスを(1,0,0)シフト
   * @param [in] index 元のインデクス
   * @param [in] h     シフト幅
   */
  inline Vec3r shift_E(const Vec3r index, const Vec3r h)
  {
    return Vec3r(index.x+h.x, index.y, index.z);
  }
  
  
  /**
   * @brief インデックスを(-1,0,0)シフト
   * @param [in] index 元のインデクス
   * @param [in] h     シフト幅
   */
  inline Vec3r shift_W(const Vec3r index, const Vec3r h)
  {
    return Vec3r(index.x-h.x, index.y, index.z  );
  }
  
  
  /**
   * @brief インデックスを(0,1,0)シフト
   * @param [in] index 元のインデクス
   * @param [in] h     シフト幅
   */
  inline Vec3r shift_N(const Vec3r index, const Vec3r h)
  {
    return Vec3r(index.x, index.y+h.y, index.z);
  }
  
  
  /**
   * @brief インデックスを(0,-1,0)シフト
   * @param [in] index 元のインデクス
   * @param [in] h     シフト幅
   */
  inline Vec3r shift_S(const Vec3r index, const Vec3r h)
  {
    return Vec3r(index.x, index.y-h.y, index.z);
  }
  
  
  /**
   * @brief インデックスを(0,0,1)シフト
   * @param [in] index 元のインデクス
   * @param [in] h     シフト幅
   */
  inline Vec3r shift_T(const Vec3r index, const Vec3r h)
  {
    return Vec3r(index.x, index.y, index.z+h.z);
  }
  
  
  /**
   * @brief インデックスを(0,0,-1)シフト
   * @param [in] index 元のインデクス
   * @param [in] h     シフト幅
   */
  inline Vec3r shift_B(const Vec3r index, const Vec3r h)
  {
    return Vec3r(index.x, index.y, index.z-h.z);
  }
  
  
  
  // サブセルのペイント
  int SubCellFill(REAL_TYPE* svf,
                  int* smd,
                  const int dir,
                  const int refID,
                  const REAL_TYPE refVf);
  
  
  // サブセルのポリゴン含有テスト
  int SubCellIncTest(REAL_TYPE* svf,
                     int* smd,
                     const int ip,
                     const int jp,
                     const int kp,
                     const Vec3r pch,
                     const string m_pg,
                     MPIPolylib* PL,
                     const int m_NoCompo);
  
  
  // sub-division
  void SubDivision(REAL_TYPE* svf,
                   int* smd,
                   const int ip,
                   const int jp,
                   const int kp,
                   int* d_mid,
                   const MediumList* mat,
                   REAL_TYPE* d_pvf,
                   const int m_NoCompo);
  
  /**
   * @brief 交点情報をアップデート
   * @param [in]     A   線分ABの端点
   * @param [in]     B   線分ABの端点
   * @param [in]     pl  平面PLの係数 ax+by+cz+d=0
   * @param [in,out] cut 量子化交点距離情報
   * @param [in,out] bid 交点ID情報
   * @param [in]     dir テストする方向
   * @param [in]     pid polygon id
   * @retval 新規交点の数
   * @note 短い距離を記録
   */
  inline unsigned updateCut(const Vec3r A,
                            const Vec3r B,
                            const REAL_TYPE pl[4],
                            long long& cut,
                            int& bid,
                            const int dir,
                            const int pid)
  {
    unsigned count = 0;
    
    // 交点座標 >> 使わない
    Vec3r X;
    
    // 交点計算
    REAL_TYPE t = intersectLineByPlane(X, A, B, pl);
    
    // 9bit幅の量子化 第2項目は調整パラメータ
    int r = quantize9(t);
    
    bool record = false;
    
    if ( 0.0 <= t && t <= 1.0 )
    {
      // 交点が記録されていない場合 >> 新規記録
      if ( ensCut(cut, dir) == 0 )
      {
        record = true;
        count = 1;
      }
      else // 交点が既に記録されている場合 >> 短い方を記録
      {
        if ( r < getBit9(cut, dir)) record = true;
      }
    }
    
    if ( record )
    {
      setBit5(bid, pid, dir);
      setBit10(cut, r, dir);
      printf("%10.6f %6d dir=%d\n", t,r, dir);
    }
    
    return count;
  }
  
  
  
  
public:
  
  // ポリゴングループの座標値からboxを計算する
  void calcBboxFromPolygonGroup(MPIPolylib* PL,
                                PolygonProperty* PG,
                                const int m_NoPolyGrp);
  
  
  // ガイドセルのIDをbcdからmidに転写
  void copyIDonGuide(const int face, const int* bcd, int* mid);
  
  
  // 交点計算を行い、量子化
  void quantizeCut(long long* cut,
                   int* bid,
                   MPIPolylib* PL,
                   PolygonProperty* PG);
  
  
  // bcd[]内にあるm_idのセルを数える
  unsigned long countCellB(const int* bcd,
                           const int m_id,
                           const bool painted=true,
                           const int* Dsize=NULL);
  
  
  // フィル操作
  void fill(FILE* fp,
            CompoList* cmp,
            MediumList* mat,
            int* d_bcd,
            long long* d_cut,
            int* d_bid,
            const int m_NoCompo);

  
  // カットID情報に基づく流体媒質のフィルを実行
  unsigned long fillByBid(int* bid,
                          int* bcd,
                          long long* cut,
                          const int tgt_id,
                          unsigned long& substituted,
                          const int m_NoCompo,
                          const int* Dsize=NULL);
  
  
  // 未ペイントセルをFLUIDでフィル
  unsigned long fillByFluid(int* bcd,
                            const int fluid_id,
                            const int* bid,
                            const int* Dsize=NULL);
  
  
  // 交点が定義点にある場合にそのポリゴンのエントリ番号でフィルする
  unsigned long fillCutOnCellCenter(int* bcd,
                                    const int* bid,
                                    const long long* cut,
                                    const int* Dsize=NULL);
  
  
  // シード点をbcd[]にペイントする
  unsigned long fillSeedBcd(int* bcd,
                            const int face,
                            const int target,
                            const int* bid,
                            const int* Dsize=NULL);
  
  
  // フィルパラメータを取得
  void getFillParam(TextParser* tpCntl);
  
  /**
   * @brief ベクトルの最小成分
   * @param [in,out] mn 比較して小さい成分
   * @param [in]     p  参照ベクトル
   */
  static inline void get_min(Vec3r& mn, const Vec3r p)
  {
    mn.x = (mn.x < p.x) ? mn.x : p.x;
    mn.y = (mn.y < p.y) ? mn.y : p.y;
    mn.z = (mn.z < p.z) ? mn.z : p.z;
  }
  
  
  /**
   * @brief ベクトルの最大値成分
   * @param [in,out] mx 比較して大きい成分
   * @param [in]     p  参照ベクトル
   */
  static inline void get_max(Vec3r& mx, const Vec3r p)
  {
    mx.x = (mx.x > p.x) ? mx.x : p.x;
    mx.y = (mx.y > p.y) ? mx.y : p.y;
    mx.z = (mx.z > p.z) ? mx.z : p.z;
  }
  
  
  /**
   * @brief 平面と線分の交点を求める
   * @param [out]  X   平面P上の交点Xの座標
   * @param [in]   A   線分ABの端点
   * @param [in]   B   線分ABの端点
   * @param [in]   PL  平面PLの係数 ax+by+cz+d=0
   * @retval 平面P上の交点XのAからの距離, 負値の場合は交点が無い
   * @see http://www.sousakuba.com/Programming/gs_plane_line_intersect.html
   */
  static REAL_TYPE intersectLineByPlane(Vec3r& X, const Vec3r A, const Vec3r B, const REAL_TYPE PL[4]);
  
  
  // FIllIDとSeedIDをセット
  void setFillMedium(MediumList* mat, const int m_NoMedium);
  
  
  // サブサンプリング
  void SubSampling(FILE* fp,
                   MediumList* mat,
                   int* d_mid,
                   REAL_TYPE* d_pvf,
                   MPIPolylib* PL,
                   const int m_NoCompo);
  
  
  // ポリゴンの水密化
  void SeedFilling(FILE* fp,
                   CompoList* cmp,
                   MediumList* mat,
                   int* d_mid,
                   MPIPolylib* PL,
                   PolygonProperty* PG,
                   const int m_NoCompo);
  
  
  // @brief 再分割数を設定
  // @param [in] num 再分割数
  void setSubDivision(int num)
  {
    if ( num < 1 ) Exit(0);
    NumSuvDiv = num;
  }

};

#endif // _FFV_GEOM_H_