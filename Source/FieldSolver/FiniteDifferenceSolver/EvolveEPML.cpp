/* Copyright 2020 Remi Lehe
 *
 * This file is part of WarpX.
 *
 * License: BSD-3-Clause-LBNL
 */
#include "FieldSolver/FiniteDifferenceSolver/FiniteDifferenceSolver.H"

#include "BoundaryConditions/PML.H"
#include "BoundaryConditions/PMLComponent.H"
#include "BoundaryConditions/PML_current.H"
#ifndef WARPX_DIM_RZ
#   include "FieldSolver/FiniteDifferenceSolver/FiniteDifferenceAlgorithms/CartesianYeeAlgorithm.H"
#   include "FieldSolver/FiniteDifferenceSolver/FiniteDifferenceAlgorithms/CartesianCKCAlgorithm.H"
#   include "FieldSolver/FiniteDifferenceSolver/FiniteDifferenceAlgorithms/CartesianNodalAlgorithm.H"
#else
#   include "FieldSolver/FiniteDifferenceSolver/FiniteDifferenceAlgorithms/CylindricalYeeAlgorithm.H"
#endif
#include "Utils/TextMsg.H"
#include "Utils/WarpXAlgorithmSelection.H"
#include "Utils/WarpXConst.H"

#include <AMReX.H>
#include <AMReX_Array4.H>
#include <AMReX_Config.H>
#include <AMReX_Extension.H>
#include <AMReX_FabArray.H>
#include <AMReX_GpuContainers.H>
#include <AMReX_GpuControl.H>
#include <AMReX_GpuLaunch.H>
#include <AMReX_GpuQualifiers.H>
#include <AMReX_IndexType.H>
#include <AMReX_MFIter.H>
#include <AMReX_MultiFab.H>
#include <AMReX_REAL.H>

#include <AMReX_BaseFwd.H>

#include <array>

using namespace amrex;

/**
 * \brief Update the E field, over one timestep
 */
void FiniteDifferenceSolver::EvolveEPML (
    std::array< amrex::MultiFab*, 3 > Efield,
    std::array< amrex::MultiFab*, 3 > const Bfield,
    std::array< amrex::MultiFab*, 3 > const edge_lengths,
    amrex::MultiFab* const Ffield, amrex::Real const dt) {

   // Select algorithm (The choice of algorithm is a runtime option,
   // but we compile code for each algorithm, using templates)
#ifdef WARPX_DIM_RZ
    amrex::ignore_unused(Efield, Bfield, Ffield, dt, edge_lengths);
    amrex::Abort(Utils::TextMsg::Err(
        "PML are not implemented in cylindrical geometry."));
#else
    if (m_do_nodal) {

        EvolveEPMLCartesian <CartesianNodalAlgorithm> (
            Efield, Bfield, edge_lengths, Ffield, dt);

    } else if (m_fdtd_algo == MaxwellSolverAlgo::Yee || m_fdtd_algo == MaxwellSolverAlgo::ECT) {

        EvolveEPMLCartesian <CartesianYeeAlgorithm> (
            Efield, Bfield, edge_lengths, Ffield, dt);

    } else if (m_fdtd_algo == MaxwellSolverAlgo::CKC) {

        EvolveEPMLCartesian <CartesianCKCAlgorithm> (
            Efield, Bfield, edge_lengths, Ffield, dt);

    } else {
        amrex::Abort(Utils::TextMsg::Err("EvolveEPML: Unknown algorithm"));
    }
#endif
}


#ifndef WARPX_DIM_RZ

template<typename T_Algo>
void FiniteDifferenceSolver::EvolveEPMLCartesian (
    std::array< amrex::MultiFab*, 3 > Efield,
    std::array< amrex::MultiFab*, 3 > const Bfield,
    std::array< amrex::MultiFab*, 3 > const edge_lengths,
    amrex::MultiFab* const Ffield, amrex::Real const dt) {

    Real constexpr c2 = PhysConst::c * PhysConst::c;

    // Loop through the grids, and over the tiles within each grid
#ifdef AMREX_USE_OMP
#pragma omp parallel if (amrex::Gpu::notInLaunchRegion())
#endif
    for ( MFIter mfi(*Efield[0], TilingIfNotGPU()); mfi.isValid(); ++mfi ) {

        // Extract field data for this grid/tile
        Array4<Real> const& Ex = Efield[0]->array(mfi);
        Array4<Real> const& Ey = Efield[1]->array(mfi);
        Array4<Real> const& Ez = Efield[2]->array(mfi);
        Array4<Real> const& Bx = Bfield[0]->array(mfi);
        Array4<Real> const& By = Bfield[1]->array(mfi);
        Array4<Real> const& Bz = Bfield[2]->array(mfi);

#ifdef AMREX_USE_EB
        Array4<Real> const& lx = edge_lengths[0]->array(mfi);
        Array4<Real> const& ly = edge_lengths[1]->array(mfi);
        Array4<Real> const& lz = edge_lengths[2]->array(mfi);
#endif

        // Extract stencil coefficients
        Real const * const AMREX_RESTRICT coefs_x = m_stencil_coefs_x.dataPtr();
        int const n_coefs_x = m_stencil_coefs_x.size();
        Real const * const AMREX_RESTRICT coefs_y = m_stencil_coefs_y.dataPtr();
        int const n_coefs_y = m_stencil_coefs_y.size();
        Real const * const AMREX_RESTRICT coefs_z = m_stencil_coefs_z.dataPtr();
        int const n_coefs_z = m_stencil_coefs_z.size();

        // Extract tileboxes for which to loop
        Box const& tex  = mfi.tilebox(Efield[0]->ixType().ixType());
        Box const& tey  = mfi.tilebox(Efield[1]->ixType().ixType());
        Box const& tez  = mfi.tilebox(Efield[2]->ixType().ixType());

        // Loop over the cells and update the fields
        amrex::ParallelFor(tex, tey, tez,

            [=] AMREX_GPU_DEVICE (int i, int j, int k){
#ifdef AMREX_USE_EB
                if(lx(i, j, k) <= 0) return;
#endif

                Ex(i, j, k, PMLComp::xz) -= c2 * dt * (
                    T_Algo::DownwardDz(By, coefs_z, n_coefs_z, i, j, k, PMLComp::yx)
                  + T_Algo::DownwardDz(By, coefs_z, n_coefs_z, i, j, k, PMLComp::yz) );
                Ex(i, j, k, PMLComp::xy) += c2 * dt * (
                    T_Algo::DownwardDy(Bz, coefs_y, n_coefs_y, i, j, k, PMLComp::zx)
                  + T_Algo::DownwardDy(Bz, coefs_y, n_coefs_y, i, j, k, PMLComp::zy) );
            },

            [=] AMREX_GPU_DEVICE (int i, int j, int k){
#ifdef AMREX_USE_EB
                if(ly(i, j, k)<=0) return;
#endif

                Ey(i, j, k, PMLComp::yx) -= c2 * dt * (
                    T_Algo::DownwardDx(Bz, coefs_x, n_coefs_x, i, j, k, PMLComp::zx)
                  + T_Algo::DownwardDx(Bz, coefs_x, n_coefs_x, i, j, k, PMLComp::zy) );
                Ey(i, j, k, PMLComp::yz) += c2 * dt * (
                    T_Algo::DownwardDz(Bx, coefs_z, n_coefs_z, i, j, k, PMLComp::xy)
                  + T_Algo::DownwardDz(Bx, coefs_z, n_coefs_z, i, j, k, PMLComp::xz) );
            },

            [=] AMREX_GPU_DEVICE (int i, int j, int k){
#ifdef AMREX_USE_EB
                if(lz(i, j, k) <= 0) return;
#endif

                Ez(i, j, k, PMLComp::zy) -= c2 * dt * (
                    T_Algo::DownwardDy(Bx, coefs_y, n_coefs_y, i, j, k, PMLComp::xy)
                  + T_Algo::DownwardDy(Bx, coefs_y, n_coefs_y, i, j, k, PMLComp::xz) );
                Ez(i, j, k, PMLComp::zx) += c2 * dt * (
                    T_Algo::DownwardDx(By, coefs_x, n_coefs_x, i, j, k, PMLComp::yx)
                  + T_Algo::DownwardDx(By, coefs_x, n_coefs_x, i, j, k, PMLComp::yz) );
            }

        );

        // If F is not a null pointer, further update E using the grad(F) term
        // (hyperbolic correction for errors in charge conservation)
        if (Ffield) {
            // Extract field data for this grid/tile
            Array4<Real> const& F = Ffield->array(mfi);

            // Loop over the cells and update the fields
            amrex::ParallelFor(tex, tey, tez,

                [=] AMREX_GPU_DEVICE (int i, int j, int k){
                    Ex(i, j, k, PMLComp::xx) += c2 * dt * (
                        T_Algo::UpwardDx(F, coefs_x, n_coefs_x, i, j, k, PMLComp::x)
                      + T_Algo::UpwardDx(F, coefs_x, n_coefs_x, i, j, k, PMLComp::y)
                      + T_Algo::UpwardDx(F, coefs_x, n_coefs_x, i, j, k, PMLComp::z) );
                },
                [=] AMREX_GPU_DEVICE (int i, int j, int k){
                    Ey(i, j, k, PMLComp::yy) += c2 * dt * (
                        T_Algo::UpwardDy(F, coefs_y, n_coefs_y, i, j, k, PMLComp::x)
                      + T_Algo::UpwardDy(F, coefs_y, n_coefs_y, i, j, k, PMLComp::y)
                      + T_Algo::UpwardDy(F, coefs_y, n_coefs_y, i, j, k, PMLComp::z) );
                },
                [=] AMREX_GPU_DEVICE (int i, int j, int k){
                    Ez(i, j, k, PMLComp::zz) += c2 * dt * (
                        T_Algo::UpwardDz(F, coefs_z, n_coefs_z, i, j, k, PMLComp::x)
                      + T_Algo::UpwardDz(F, coefs_z, n_coefs_z, i, j, k, PMLComp::y)
                      + T_Algo::UpwardDz(F, coefs_z, n_coefs_z, i, j, k, PMLComp::z) );
                }
            );
        }
    }

#ifndef AMREX_USE_EB
    amrex::ignore_unused(edge_lengths);
#endif

}

#endif // corresponds to ifndef WARPX_DIM_RZ
