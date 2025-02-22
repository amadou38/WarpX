/* Copyright 2021 Modern Electron
 *
 * This file is part of WarpX.
 *
 * License: BSD-3-Clause-LBNL
 */
#ifndef WARPX_PARTICLES_COLLISION_BACKGROUNDMCCCOLLISION_H_
#define WARPX_PARTICLES_COLLISION_BACKGROUNDMCCCOLLISION_H_

#include "MCCProcess.H"
#include "Particles/MultiParticleContainer.H"
#include "Particles/Collision/CollisionBase.H"

#include <AMReX_Parser.H>
#include <AMReX_REAL.H>
#include <AMReX_Vector.H>
#include <AMReX_GpuContainers.H>

#include <memory>
#include <string>

class BackgroundMCCCollision final
    : public CollisionBase
{
public:
    BackgroundMCCCollision (std::string collision_name);

    virtual ~BackgroundMCCCollision () = default;

    amrex::Real get_nu_max (amrex::Vector<MCCProcess> const& mcc_processes);

    /** Perform the collisions
     *
     * @param cur_time Current time
     * @param dt Time step size
     * @param mypc Container of species involved
     *
     */
    void doCollisions (amrex::Real cur_time, amrex::Real dt, MultiParticleContainer* mypc) override;

    /** Perform particle conserving MCC collisions within a tile
     *
     * @param pti particle iterator
     * @param t current time
     *
     */
    void doBackgroundCollisionsWithinTile ( WarpXParIter& pti, amrex::Real t);

    /** Perform MCC ionization interactions
     *
     * @param[in] lev the mesh-refinement level
     * @param[in,out] cost pointer to (load balancing) cost corresponding to box where present particles are ionized.
     * @param[in,out] species1,species2 reference to species container used to inject
     * new particles
     * @param t current time
     *
     */
    void doBackgroundIonization (
                                 int lev,
                                 amrex::LayoutData<amrex::Real>* cost,
                                 WarpXParticleContainer& species1,
                                 WarpXParticleContainer& species2,
                                 amrex::Real t
                                 );

private:

    amrex::Vector<MCCProcess> m_scattering_processes;
    amrex::Vector<MCCProcess> m_ionization_processes;
    amrex::Gpu::DeviceVector<MCCProcess::Executor> m_scattering_processes_exe;
    amrex::Gpu::DeviceVector<MCCProcess::Executor> m_ionization_processes_exe;

    bool init_flag = false;
    bool ionization_flag = false;

    amrex::Real m_mass1;

    amrex::Real m_max_background_density = 0;
    amrex::Real m_background_mass;
    amrex::Real m_total_collision_prob;
    amrex::Real m_total_collision_prob_ioniz = 0;
    amrex::Real m_nu_max;
    amrex::Real m_nu_max_ioniz;

    amrex::Parser m_background_density_parser;
    amrex::Parser m_background_temperature_parser;

    amrex::ParserExecutor<4> m_background_density_func;
    amrex::ParserExecutor<4> m_background_temperature_func;
};

#endif // WARPX_PARTICLES_COLLISION_BACKGROUNDMCCCOLLISION_H_
