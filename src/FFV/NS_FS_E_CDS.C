//##################################################################################
//
// FFV-C : Frontflow / violet Cartesian
//
// Copyright (c) 2007-2011 VCAD System Research Program, RIKEN.
// All rights reserved.
//
// Copyright (c) 2011-2015 Institute of Industrial Science, The University of Tokyo.
// All rights reserved.
//
// Copyright (c) 2012-2015 Advanced Institute for Computational Science, RIKEN.
// All rights reserved.
//
//##################################################################################

/**
 * @file   NS_FS_E_CDS.C
 * @brief  FFV Class
 * @author aics
 */

#include "ffv.h"


// Fractional Step法でNavier-Stokes方程式を解く．距離情報近似．
void FFV::NS_FS_E_CDS()
{
  // local variables
  double flop;                         /// 浮動小数演算数
  double b_l2 = 0.0;                   /// 反復解法での定数項ベクトルのL2ノルム
  double res0_l2 = 0.0;                /// 反復解法での初期残差ベクトルのL2ノルム
  
  REAL_TYPE dt = deltaT;               /// 時間積分幅
  REAL_TYPE dh = pitch[0];    /// 空間幅
  REAL_TYPE coef = dh/dt;          /// Poissonソース項の係数
  REAL_TYPE Re = C.Reynolds;           /// レイノルズ数
  REAL_TYPE rei = C.getRcpReynolds();  /// レイノルズ数の逆数
  REAL_TYPE half = 0.5;                /// 定数
  REAL_TYPE one = 1.0;                 /// 定数
  REAL_TYPE zero = 0.0;                /// 定数
  int cnv_scheme = C.CnvScheme;        /// 対流項スキーム
  
  REAL_TYPE ltd_c = pitch[0] * C.Mach / dt; /// Limited Compressibility   (dx*M/dt)
  if ( C.BasicEqs == INCMP ) ltd_c = 0.0;
  
  // 境界処理用
  Gemini_R* m_buf = new Gemini_R [C.NoCompo+1];
  REAL_TYPE* m_snd = new REAL_TYPE [(C.NoCompo+1)*2];
  REAL_TYPE* m_rcv = new REAL_TYPE [(C.NoCompo+1)*2];

  
  int v_mode=0;
  
  LinearSolver* LSp = &LS[ic_prs1];  /// 圧力のPoisson反復
  LinearSolver* LSv = &LS[ic_vel1];  /// 粘性項のCrank-Nicolson反復
  
  // point Data
  // d_v   セルセンタ速度 v^n -> v^{n+1}
  // d_v0  セルセンタ速度 v^nの保持
  // d_vc  疑似速度ベクトル
  // d_wv  ワーク　陰解法の時の疑似速度ベクトル，射影ステップの境界条件
  // d_p   圧力 p^n -> p^{n+1}
  // d_p0  圧力 p^nの保持
  // d_ws  Poissonのソース項0　速度境界を考慮
  // d_sq Poissonのソース項1　反復毎に変化するソース項，摩擦速度，発散値, div(u)の値を保持，出力のところまでは再利用しないこと
  // d_bcd IDのビットフラグ
  // d_bcp 圧力のビットフラグ
  // d_cdf Component Directional BC Flag
  // d_cvf コンポーネントの体積率
  // d_ie0 内部エネルギー
  // d_vt  LES計算の渦粘性係数
  // d_ab0 Adams-Bashforth用のワーク
  // d_cut カット情報
  
  
  // >>> Fractional step section
  TIMING_start("NS__F_Step_Section");
  
  
  // n stepの値を保持 >> In use (d_v0, d_p0)
  TIMING_start("Copy_Array");
  U.copyS3D(d_p0, size, guide, d_p, one);
  U.copyV3D(d_v0, size, guide, d_v, one);
  TIMING_stop("Copy_Array", 0.0, 2);
  
  
  
  // 対流項と粘性項の評価 >> In use (dc_vc, dc_wv)
  switch (C.AlgorithmF) {
    case Flow_FS_EE_EE:
    case Flow_FS_AB2:
      
      flop = 0.0;
      v_mode = 1; // ?

      if ( C.LES.Calc == ON )
      {
        Hostonly_ printf("not inplemented yet. sorry:-)\n");
        Exit(0);
      }
      else
      {
        TIMING_start("Pvec_MUSCL");
        flop = 0.0;
        pvec_muscl_cds_(d_vc, size, &guide, &dh, &cnv_scheme, v00, &rei, d_v0, d_vf, d_cdf, d_bcp, d_bcd, &v_mode, d_cut, &flop);
        TIMING_stop("Pvec_MUSCL", flop);
      }

      TIMING_start("Pvec_Flux_BC");
      flop = 0.0;
      BC.modPvecFlux(d_vc, d_v0, d_cdf, CurrentTime, &C, v00, flop);
      TIMING_stop("Pvec_Flux_BC", flop);
      break;
      
    case Flow_FS_AB_CN:
      v_mode = 0;
      if ( C.LES.Calc == ON )
      {
        Hostonly_ printf("not inplemented yet. sorry:-)\n");
      }
      else
      {
        TIMING_start("Pvec_MUSCL");
        flop = 0.0;
        pvec_muscl_cds_(d_wv, size, &guide, &dh, &cnv_scheme, v00, &rei, d_v0, d_vf, d_cdf, d_bcp, d_bcd, &v_mode, d_cut, &flop);
        TIMING_stop("Pvec_MUSCL", flop);
      }
      
      TIMING_start("Pvec_Flux_BC");
      flop = 0.0;
      BC.modPvecFlux(d_wv, d_v0, d_cdf, CurrentTime, &C, v00, flop);
      TIMING_stop("Pvec_Flux_BC", flop);
      break;
      
    default:
      Exit(0);
  }

  
  // 時間積分
  switch (C.AlgorithmF)
  {
    case Flow_FS_EE_EE:
      TIMING_start("Pvec_Euler_Explicit");
      flop = 0.0;
      euler_explicit_ (d_vc, size, &guide, &dt, d_v0, d_bcd, &flop);
      TIMING_stop("Pvec_Euler_Explicit", flop);
      break;
      
    case Flow_FS_AB2:
      TIMING_start("Pvec_Adams_Bashforth");
      flop = 0.0;
      if ( Session_CurrentStep == 1 ) // 初期とリスタート後，1ステップめ
      {
        euler_explicit_ (d_vc, size, &guide, &dt, d_v0, d_bcd, &flop);
      }
      else
      {
        ab2_(d_vc, size, &guide, &dt, d_v0, d_abf, d_bcd, v00, &flop);
      }
      TIMING_stop("Pvec_Adams_Bashforth", flop);
      break;
      
    case Flow_FS_AB_CN: // 未対応20110918
      TIMING_start("Pvec_AB_CN");
      flop = 0.0;
      if ( Session_CurrentStep == 1 )
      {
        euler_explicit_ (d_wv, size, &guide, &dt, d_v0, d_bcd, &flop);
      }
      else
      {
        ab2_(d_wv, size, &guide, &dt, d_v0, d_abf, d_bcd, v00, &flop);
      }
      TIMING_stop("Pvec_AB_CN", flop);
      

      // implicit part
      break;
      
    default:
      Exit(0);
  }
  

  
  // FORCINGコンポーネントの疑似速度ベクトルの方向修正
  if ( C.EnsCompo.forcing == ON )
  {
    TIMING_start("Pvec_Forcing");
    flop = 0.0;
    BC.mod_Pvec_Forcing(d_vc, d_v, d_bcd, d_cvf, v00, dt, flop);
    TIMING_stop("Pvec_Forcing", flop);
  }
  
  // 浮力項
  if ( C.isHeatProblem() && (C.Mode.Buoyancy == BOUSSINESQ) )
  {
    TIMING_start("Pvec_Buoyancy");
    REAL_TYPE dgr = dt*C.Grashof*rei*rei;
    flop = 3.0;
    ps_buoyancy_(d_vc, size, &guide, &dgr, d_ie0, d_bcd, &C.NoCompo, mat_tbl, &flop);
    TIMING_stop("Pvec_Buoyancy", flop);
  }
  
  
  // 疑似ベクトルの境界条件
  TIMING_start("Pvec_BC");
  BC.OuterVBCfacePrep(d_vc, d_v0, d_cdf, dt, &C, ensPeriodic, Session_CurrentStep);
  BC.InnerVBCperiodic(d_vc, d_bcd);
  TIMING_stop("Pvec_BC");
  
  
  // 疑似ベクトルの同期
  if ( numProc > 1 )
  {
    TIMING_start("Sync_Pvec");
    if ( paraMngr->BndCommV3D(d_vc, size[0], size[1], size[2], guide, guide, procGrp) != CPM_SUCCESS ) Exit(0);
    TIMING_stop("Sync_Pvec", face_comm_size*3.0*guide*sizeof(REAL_TYPE)); // ガイドセル数 x ベクトル
  }
  

  
  // Crank-Nicolson Iteration
  if ( C.AlgorithmF == Flow_FS_AB_CN )
  {
    TIMING_start("Copy_Array");
    U.copyV3D(d_wv, size, guide, d_vc, one);
    TIMING_stop("Copy_Array");
    
    for (LSv->setLoopCount(0); LSv->getLoopCount()< LSv->getMaxIteration(); LSv->incLoopCount())
    {
      //CN_Itr(LSv);
      if ( LSv->isErrConverged() || LSv->isResConverged() ) break;
    }
  }
  
  
  TIMING_stop("NS__F_Step_Section");
  // <<< Fractional step section
  

  
  // Poissonのソース部分
  // >>> Poisson Source section
  TIMING_start("Poisson__Source_Section");
  
  
  // 非VBC面に対してのみ，セルセンターの値から発散量を計算
  TIMING_start("Divergence_of_Pvec");
  flop = 0.0;
  divergence_cds_(d_ws, size, &guide, &coef, d_vc, d_cdf, d_bcd, d_cut, v00, &flop);
  TIMING_stop("Divergence_of_Pvec", flop);
  
  
  // Poissonソース項の速度境界条件（VBC）面による修正
  TIMING_start("Poisson_Src_VBC");
  flop = 0.0;
  BC.modPsrcVBC(d_ws, d_cdf, CurrentTime, &C, v00, d_vf, d_vc, d_v0, dt, flop);
  TIMING_stop("Poisson_Src_VBC", flop);
  
  
  
  // (Neumann_BCType_of_Pressure_on_solid_wall == grad_NS)　のとき，\gamma^{N2}の処理
  //hogehoge
  
  // 定数項bの自乗和　b_l2
  if ( C.BasicEqs == INCMP )
  {
    TIMING_start("Poisson_Src_Norm");
    b_l2 = 0.0;
    flop = 0.0;
    blas_calc_b_(&b_l2, d_b, d_ws, d_bcp, size, &guide, pitch, &dt, &flop);
    TIMING_stop("Poisson_Src_Norm", flop);
  }
  else if ( C.BasicEqs == LTDCMP )
  {
    TIMING_start("Poisson_Src_Norm");
    b_l2 = 0.0;
    flop = 0.0;
    blas_calc_b_lc_(&b_l2, d_b, d_ws, d_bcp, size, &guide, pitch, &dt, d_p0, &ltd_c, &flop);
    TIMING_stop("Poisson_Src_Norm", flop);
  }
  else
  {
    Exit(0);
  }
  
  
  if ( numProc > 1 )
  {
    TIMING_start("A_R_Poisson_Src_L2");
    double m_tmp = b_l2;
    if ( paraMngr->Allreduce(&m_tmp, &b_l2, 1, MPI_SUM, procGrp) != CPM_SUCCESS ) Exit(0);
    TIMING_stop("A_R_Poisson_Src_L2", 2.0*numProc*sizeof(double) ); // 双方向 x ノード数
  }
  
  // L2 norm of b vector
  b_l2 = sqrt(b_l2);
  
  
  
  // Initial residual
  if ( LSp->getResType() == nrm_r_r0 )
  {
    TIMING_start("Poisson_Init_Res");
    res0_l2 = 0.0;
    flop = 0.0;
    blas_calc_r2_(&res0_l2, d_p, d_b, d_bcp, size, &guide, pitch, &ltd_c, &flop);
    TIMING_stop("Poisson_Init_Res", flop);
    
    if ( numProc > 1 )
    {
      TIMING_start("A_R_Poisson_Init_Res_L2");
      double m_tmp = res0_l2;
      if ( paraMngr->Allreduce(&m_tmp, &res0_l2, 1, MPI_SUM, procGrp) != CPM_SUCCESS ) Exit(0);
      TIMING_stop("A_R_Poisson_Init_Res_L2", 2.0*numProc*sizeof(double) ); // 双方向 x ノード数
    }
    
    res0_l2 = sqrt(res0_l2);
  }
  
  TIMING_stop("Poisson__Source_Section");
  // <<< Poisson Source section
  
  
  
  // VP-Iteration
  // >>> Poisson Iteration section
  TIMING_start("VP-Iteration_Section");
  

  for (LSp->setLoopCount(0); LSp->getLoopCount() <= LSp->getMaxIteration(); LSp->incLoopCount())
  {
    
    // 反復ソース項のゼロクリア => src1
    //TIMING_start(tm_assign_const);
    U.initS3D(d_sq, size, guide, zero);
    //TIMING_stop(tm_assign_const, 0.0);
    
    // Forcingコンポーネントによるソース項の寄与分
    if ( C.EnsCompo.forcing == ON )
    {
      //TIMING_start(tm_force_src);
      flop=0.0;
      BC.mod_Psrc_Forcing(d_sq, d_v, d_bcd, d_cvf, v00, component_array, flop);
      //TIMING_stop(tm_force_src, flop);
    }
    
    // 内部周期境界部分のディリクレソース項
    //TIMING_start(tm_prdc_src);
    //BC.InnerPrdc_Src(dc_wk2, dc_p, dc_bcd);
    //TIMING_stop(tm_prdc_src, flop);
    

    // 線形ソルバー
    switch (LSp->getLS())
    {
      case SOR:
        LSp->PointSOR(d_p, d_b, dt, LSp->getMaxIteration(), b_l2, res0_l2); // return x^{m+1} - x^m
        break;
        
      case SOR2SMA:
        LSp->SOR2_SMA(d_p, d_b, dt, LSp->getMaxIteration(), b_l2, res0_l2); // return x^{m+1} - x^m
        break;
        
      //case GMRES:
      //  Fgmres(LSp, rhs_nrm, res_init); // return ?
      //  break;
        
      default:
        printf("\tInvalid Linear Solver for Pressure\n");
        Exit(0);
        break;
    }
    

    
    // 速度のスカラポテンシャルによる射影と発散値 src1は，反復毎のソース項をワークとして利用
    TIMING_start("Projection_Velocity");
    flop = 0.0;
    update_vec_cds_(d_v, d_dv, size, &guide, &dt, &dh, d_vc, d_p, d_bcp, d_cdf, d_bcd, d_cut, v00, &flop);
    TIMING_stop("Projection_Velocity", flop);
    
    // セルフェイス速度の境界条件による修正
    TIMING_start("Projection_Velocity_BC");
    flop=0.0;
    BC.modDivergence(d_dv, d_cdf, CurrentTime, &C, v00, m_buf, flop);
    TIMING_stop("Projection_Velocity_BC", flop);

    
    
    // セルフェイス速度の境界条件の通信部分
    if ( C.EnsCompo.outflow == ON )
    {
      if ( numProc > 1 )
      {
        for (int n=1; n<=C.NoCompo; n++)
        {
          m_snd[2*n]   = m_rcv[2*n]   = m_buf[n].p0; // 積算速度
          m_snd[2*n+1] = m_rcv[2*n+1] = m_buf[n].p1; // 積算回数
        }
        
        TIMING_start("A_R_Projection_VBC");
        if ( paraMngr->Allreduce(m_snd, m_rcv, 2*(C.NoCompo+1), MPI_SUM, procGrp) != CPM_SUCCESS ) Exit(0);
        TIMING_stop("A_R_Projection_VBC", 2.0*C.NoCompo*numProc*sizeof(REAL_TYPE)*2.0 ); // 双方向 x ノード数 x 変数
        
        for (int n=1; n<=C.NoCompo; n++)
        {
          m_buf[n].p0 = m_rcv[2*n];
          m_buf[n].p1 = m_rcv[2*n+1];
        }
      }
      
      for (int n=1; n<=C.NoCompo; n++)
      {
        if ( cmp[n].getType() == OUTFLOW )
        {
          cmp[n].val[var_Velocity] = m_buf[n].p0 / m_buf[n].p1; // 無次元平均流速
        }
      }
    }
    
    // Forcingコンポーネントによる速度と発散値の修正
    if ( C.EnsCompo.forcing == ON )
    {
      TIMING_start("Projection_Forcing");
      flop=0.0;
      BC.mod_Vdiv_Forcing(d_v, d_bcd, d_cvf, d_dv, dt, v00, m_buf, component_array, flop);
      TIMING_stop("Projection_Forcing", flop);
      
      // 通信部分
      if ( numProc > 1 )
      {
        for (int n=1; n<=C.NoCompo; n++)
        {
          m_snd[2*n]   = m_rcv[2*n]   = m_buf[n].p0; // 積算速度
          m_snd[2*n+1] = m_rcv[2*n+1] = m_buf[n].p1; // 積算圧力損失
        }
        
        TIMING_start("A_R_Projection_Forcing");
        if ( paraMngr->Allreduce(m_snd, m_rcv, 2*(C.NoCompo+1), MPI_SUM, procGrp) != CPM_SUCCESS ) Exit(0);
        TIMING_stop("A_R_Projection_Forcing", 2.0*(C.NoCompo+1)*numProc*sizeof(REAL_TYPE)*2.0);
        
        for (int n=1; n<=C.NoCompo; n++)
        {
          m_buf[n].p0 = m_rcv[2*n];
          m_buf[n].p1 = m_rcv[2*n+1];
        }
      }
      
      for (int n=1; n<=C.NoCompo; n++)
      {
        if ( cmp[n].isFORCING() )
        {
          REAL_TYPE aa = (REAL_TYPE)cmp[n].getElement();
          cmp[n].val[var_Velocity] = m_buf[n].p0 / aa; // 平均速度
          cmp[n].val[var_Pressure] = m_buf[n].p1 / aa; // 平均圧力損失量
        }
      }
    }

    // 周期型の速度境界条件
    TIMING_start("Velocity_BC");
    BC.InnerVBCperiodic(d_v, d_bcd);
    TIMING_stop("Velocity_BC");
    
 
    
    // ノルムの計算
    NormDiv(d_dv, dt);
    
    /* Forcingコンポーネントによる速度の方向修正(収束判定から除外)  >> TEST
     TIMING_start(tm_prj_frc_dir);
     flop=0.0;
     BC.mod_Dir_Forcing(d_v, d_bcd, d_cvf, v00, flop);
     TIMING_stop(tm_prj_frc_dir, flop);
     */
    
    // 収束判定　性能測定モードのときは収束判定を行わない
    //if ( (C.Hide.PM_Test == OFF) && LSd->isConverged() ) break;
  } // end of iteration
  
  TIMING_stop("VP-Iteration_Section", 0.0);
  // <<< Poisson Iteration section
  
  
  
  /// >>> NS Loop post section
  TIMING_start("NS__Loop_Post_Section");
  
  // 同期
  if ( numProc > 1 )
  {
    TIMING_start("Sync_Velocity");
    if ( paraMngr->BndCommV3D(d_v, size[0], size[1], size[2], guide, guide, procGrp) != CPM_SUCCESS ) Exit(0);
    TIMING_stop("Sync_Velocity", face_comm_size*guide*3.0*sizeof(REAL_TYPE));
  }
  
  // 外部領域境界面での速度や流量を計算 > 外部流出境界条件の移流速度に利用
  TIMING_start("Domain_Monitor");
  DomainMonitor(BC.exportOBC(), &C);
  TIMING_stop("Domain_Monitor");
  
  
  /* 非同期にして隠す
  if (C.LES.Calc==ON)
  {
    TIMING_start(tm_LES_eddy);
    flop = 0.0;
    eddy_viscosity_(d_vt, size, &guide, &dh, &C.Reynolds, &C.LES.Cs, d_v, d_cdf, range_Ut, range_Yp, v00);
    TIMING_stop(tm_LES_eddy, flop);
    
    if ( numProc > 1 )
    {
      TIMING_start(tm_LES_eddy_comm);
      if ( paraMngr->BndCommS3D(d_vt, size[0], size[1], size[2], guide, guide, procGrp) != CPM_SUCCESS ) Exit(0);
      TIMING_stop(tm_LES_eddy_comm, face_comm_size*guide*sizeof(REAL_TYPE));
    }
  }*/
  

  
  TIMING_stop("NS__Loop_Post_Section", 0.0);
  // >>> NS loop post section
  
  // 後始末
  if ( m_buf ) delete [] m_buf;
  if ( m_snd ) delete [] m_snd;
  if ( m_rcv ) delete [] m_rcv;
  
}
