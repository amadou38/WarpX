#include "Utils/WarpXProfilerWrapper.H"
#include "FlushFormatSensei.H"

#include "WarpX.H"

#ifdef AMREX_USE_SENSEI_INSITU
# include <AMReX_AmrMeshParticleInSituBridge.H>
#endif

FlushFormatSensei::FlushFormatSensei () :
    m_insitu_config(), m_insitu_pin_mesh(0), m_insitu_bridge(nullptr),
    m_amr_mesh(nullptr)
{}

FlushFormatSensei::FlushFormatSensei (amrex::AmrMesh *amr_mesh,
    std::string diag_name) :
    m_insitu_config(), m_insitu_pin_mesh(0), m_insitu_bridge(nullptr),
    m_amr_mesh(amr_mesh)
{
#ifndef AMREX_USE_SENSEI_INSITU
    amrex::ignore_unused(m_insitu_pin_mesh, m_insitu_bridge, m_amr_mesh, diag_name);
#else
    amrex::ParmParse pp_diag_name(diag_name);

    pp_diag_name.query("sensei_config", m_insitu_config);
    pp_diag_name.query("sensei_pin_mesh", m_insitu_pin_mesh);

    m_insitu_bridge = new amrex::AmrMeshParticleInSituBridge;
    m_insitu_bridge->setEnabled(true);
    m_insitu_bridge->setConfig(m_insitu_config);
    m_insitu_bridge->setPinMesh(m_insitu_pin_mesh);
    if (!m_amr_mesh || m_insitu_bridge->initialize())
    {
        amrex::ErrorStream() << "FlushFormtSensei::FlushFormatSensei : "
            "Failed to initialize the in situ bridge." << std::endl;

        amrex::Abort();
    }
    m_insitu_bridge->setFrequency(1);
#endif
}

FlushFormatSensei::~FlushFormatSensei ()
{
#ifdef AMREX_USE_SENSEI_INSITU
    delete m_insitu_bridge;
#endif
}

void
FlushFormatSensei::WriteToFile (
    const amrex::Vector<std::string> varnames,
    const amrex::Vector<amrex::MultiFab>& mf,
    amrex::Vector<amrex::Geometry>& geom,
    const amrex::Vector<int> iteration, const double time,
    const amrex::Vector<ParticleDiag>& particle_diags,
    int nlev, const std::string prefix, int file_min_digits,
    bool plot_raw_fields, bool plot_raw_fields_guards,
    bool isBTD, int snapshotID,
    const amrex::Geometry& full_BTD_snapshot, bool isLastBTDFlush,
    const amrex::Vector<int>& totalParticlesFlushedAlready) const
{
    amrex::ignore_unused(
        geom, nlev, prefix, file_min_digits,
        plot_raw_fields, plot_raw_fields_guards,
        isBTD, snapshotID, full_BTD_snapshot,
        isLastBTDFlush, totalParticlesFlushedAlready);

#ifndef AMREX_USE_SENSEI_INSITU
    amrex::ignore_unused(varnames, mf, iteration, time, particle_diags);
#else
    WARPX_PROFILE("FlushFormatSensei::WriteToFile()");

    amrex::Vector<amrex::MultiFab> *mf_ptr =
        const_cast<amrex::Vector<amrex::MultiFab>*>(&mf);

    auto particles = particle_diags[0].getParticleContainer();
    bool didUpdate = m_insitu_bridge->update(
        iteration[0], time, m_amr_mesh,{mf_ptr}, {varnames},
        particles, {}, {}, {{"u",{0,1,2}}}, {});

    if (didUpdate)
    {
        amrex::ErrorStream() << "FlushFormatSensei::WriteToFile : "
            "Failed to update the in situ bridge." << std::endl;

        amrex::Abort();
    }
#endif
}

void
FlushFormatSensei::WriteParticles (
    const amrex::Vector<ParticleDiag>& particle_diags) const
{
    amrex::ignore_unused(particle_diags);
#ifdef AMREX_USE_SENSEI_INSITU
    amrex::ErrorStream() << "FlushFormatSensei::WriteParticles : "
        "Not yet implemented." << std::endl;

    amrex::Abort();
#endif
}
