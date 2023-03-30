// Copyright (c) Lawrence Livermore National Security, LLC and other Conduit
// Project developers. See top-level LICENSE AND COPYRIGHT files for dates and
// other details. No copyright assignment is required to contribute to Conduit.

//-----------------------------------------------------------------------------
///
/// file: t_relay_io_silo.cpp
///
//-----------------------------------------------------------------------------

#include "conduit_relay.hpp"
#include "conduit_relay_io_silo.hpp"
#include "conduit_blueprint.hpp"
#include "t_config.hpp"

#include <iostream>
#include "gtest/gtest.h"

using namespace conduit;
using namespace conduit::relay;


std::string
relay_test_silo_data_path(const std::string &test_fname)
{
    std::string res = utils::join_path(CONDUIT_T_SRC_DIR, "relay");
    res = utils::join_file_path(res, "data");
    res = utils::join_file_path(res, "silo");
    return utils::join_file_path(res, test_fname);
}


TEST(conduit_relay_io_silo, conduit_silo_cold_storage)
{
    uint32 a_val = 20;
    uint32 b_val = 8;
    uint32 c_val = 13;

    Node n;
    n["a"] = a_val;
    n["b"] = b_val;
    n["c"] = c_val;

    EXPECT_EQ(n["a"].as_uint32(), a_val);
    EXPECT_EQ(n["b"].as_uint32(), b_val);
    EXPECT_EQ(n["c"].as_uint32(), c_val);

    io::silo_write(n,"tout_cold_storage_test.silo:myobj");

    Node n_load;
    io::silo_read("tout_cold_storage_test.silo:myobj",n_load);

    EXPECT_EQ(n_load["a"].as_uint32(), a_val);
    EXPECT_EQ(n_load["b"].as_uint32(), b_val);
    EXPECT_EQ(n_load["c"].as_uint32(), c_val);
}

TEST(conduit_relay_io_silo, conduit_silo_cold_storage_generic_iface)
{
    uint32 a_val = 20;
    uint32 b_val = 8;
    uint32 c_val = 13;

    Node n;
    n["a"] = a_val;
    n["b"] = b_val;
    n["c"] = c_val;

    EXPECT_EQ(n["a"].as_uint32(), a_val);
    EXPECT_EQ(n["b"].as_uint32(), b_val);
    EXPECT_EQ(n["c"].as_uint32(), c_val);

    io::save(n, "tout_cold_storage_test_generic_iface.silo:myobj");

    Node n_load;
    io::load("tout_cold_storage_test_generic_iface.silo:myobj",n_load);

    EXPECT_EQ(n_load["a"].as_uint32(), a_val);
    EXPECT_EQ(n_load["b"].as_uint32(), b_val);
    EXPECT_EQ(n_load["c"].as_uint32(), c_val);
}

// test reading in a handful of different silo files
TEST(conduit_relay_io_silo, load_mesh_geometry)
{

    // TODO: all these files are in overlink symlink format.
    // Symlinks may break on Windows (?)
    // Could make them overlink format without the symlink.
    // But would require modifying the files.
    std::vector<std::string> filename_vec = {
        "box2d.silo",
        "box3d.silo",
        "diamond.silo",
        // TODO: rename these files to be more descriptive.
        // would also require modifying the paths stored within the files,
        // and re-symlinking
        "testDisk2D_a.silo",
        "donordiv.s2_materials2.silo",
        "donordiv.s2_materials3.silo"
    };
    std::vector<int> dims_vec = {2, 3, 2, 2, 2, 2};
    std::vector<int> coordset_length_vec = {4, 8, 36, 1994, 16, 961};
    std::vector<int> topology_length_vec = {1, 1, 33, 1920, 9, 900};
    for (int i = 0; i < filename_vec.size(); ++i) 
    {
        Node mesh, info;
        std::string input_file = relay_test_silo_data_path(filename_vec.at(i));
        io::silo::load_mesh(input_file, mesh);

        EXPECT_TRUE(blueprint::mesh::verify(mesh, info));
        EXPECT_EQ(blueprint::mesh::number_of_domains(mesh), 1);

        const Node &domain = *blueprint::mesh::domains(mesh).front();
        EXPECT_TRUE(domain.has_child("coordsets"));
        EXPECT_EQ(domain["coordsets"].number_of_children(), 1);
        EXPECT_TRUE(domain.has_child("topologies"));
        EXPECT_EQ(domain["topologies"].number_of_children(), 1);

        { // Coordset Validation //
            const Node &cset = domain["coordsets"].child(0);
            EXPECT_EQ(blueprint::mesh::coordset::dims(cset), dims_vec.at(i));
            EXPECT_EQ(blueprint::mesh::coordset::length(cset), coordset_length_vec.at(i));
            EXPECT_TRUE(blueprint::mesh::coordset::_explicit::verify(cset, info));
        }

        { // Topology Validation //
            const Node &topo = domain["topologies"].child(0);
            EXPECT_EQ(blueprint::mesh::topology::dims(topo), dims_vec.at(i));
            EXPECT_EQ(blueprint::mesh::topology::length(topo), topology_length_vec.at(i));
            EXPECT_TRUE(blueprint::mesh::topology::unstructured::verify(topo, info));
        }
    }
}


// TODO: add tests for matsets
TEST(conduit_relay_io_silo, save_mesh_geometry_basic)
{
    const std::vector<std::string> mesh_types = {
        "uniform", "rectilinear", "structured",
        "tris", "quads"};
    for (int i = 0; i < mesh_types.size(); ++i)
    {
        for (int nx = 2; nx < 4; ++nx)
        {
            Node save_mesh;
            int npts_z = 0;
            if (mesh_types[i] != "tris" && mesh_types[i] != "quads")
            {
                npts_z = (nx - 2) * 2 + (mesh_types[i] == "structured" ? 1 : 0);
            }
            blueprint::mesh::examples::basic(mesh_types[i], 
                nx, nx, npts_z, save_mesh);

            Node info;
            
            io::silo::save_mesh(save_mesh, "basic");

            Node load_mesh;
            io::silo::load_mesh("basic.root", load_mesh);
            
            EXPECT_TRUE(blueprint::mesh::verify(load_mesh,info));

            // The silo conversion will transform uniform to rectilinear
            // so we will do the same to allow the diff to succeed
            if (mesh_types[i] == "uniform")
            {
                Node save_mesh_rect;
                Node &save_mesh_rect_coords = save_mesh_rect["coordsets"]["coords"];
                Node &save_mesh_rect_topo = save_mesh_rect["topologies"]["mesh"];
                blueprint::mesh::topology::uniform::to_rectilinear(
                    save_mesh["topologies"]["mesh"], 
                    save_mesh_rect_topo, save_mesh_rect_coords);
                save_mesh["topologies"]["mesh"].set(save_mesh_rect_topo);
                save_mesh["coordsets"]["coords"].set(save_mesh_rect_coords);
            }

            // The Blueprint to Silo transformation changes several names 
            // and some information is lost. We manually make changes so 
            // that the diff will pass.
            save_mesh["coordsets"].rename_child("coords", "mesh_mesh");
            save_mesh["topologies"].rename_child("mesh", "mesh_mesh");
            save_mesh["fields"].rename_child("field", "mesh_field");
            save_mesh["topologies"]["mesh_mesh"]["coordset"].reset();
            save_mesh["topologies"]["mesh_mesh"]["coordset"] = "mesh_mesh";
            save_mesh["fields"]["mesh_field"]["topology"].reset();
            save_mesh["fields"]["mesh_field"]["topology"] = "mesh_mesh";
            save_mesh["fields"]["mesh_field"].remove_child("volume_dependent");

            // the loaded mesh will be in the multidomain format
            // (it will be a list containing a single mesh domain)
            // but the saved mesh is in the single domain format
            EXPECT_EQ(load_mesh.number_of_children(), 1);
            EXPECT_EQ(load_mesh[0].number_of_children(), save_mesh.number_of_children());

            EXPECT_FALSE(load_mesh[0].diff(save_mesh, info));
        }
    }
}

// TODO: make this pass?
// right now multidomain meshes are read out as a list, but
// blueprint specifies that multidomain meshes are objects.
// this is one reason the test fails.
// Problem: in overlink, all domains are named the same ('MESH')
TEST(conduit_relay_io_silo, save_mesh_geometry_spiral)
{
    for (int ndomains = 3; ndomains < 4; ndomains ++)
    {
        Node save_mesh;
        blueprint::mesh::examples::spiral(ndomains, save_mesh);
        for (index_t child = 0; child < save_mesh.number_of_children(); ++child)
        {
            save_mesh[child].remove("state"); // TODO uncomment this and add functionality for it
        }

        // TODO there is a bug on the save side only multimeshes and multivars are being saved

        io::silo::save_mesh(save_mesh, "spiral");
        Node load_mesh;
        // io::silo::load_mesh("spiral.root", load_mesh);

        Node info;

        // EXPECT_TRUE(blueprint::mesh::verify(load_mesh,info));

        // for (index_t child = 0; child < save_mesh.number_of_children(); ++child)
        // {
        //     // The Blueprint to Silo transformation changes several names 
        //     // and some information is lost. We manually make changes so 
        //     // that the diff will pass.
        //     save_mesh[child]["coordsets"].rename_child("coords", "topo");
        //     save_mesh[child]["topologies"]["topo"]["coordset"].reset();
        //     save_mesh[child]["topologies"]["topo"]["coordset"] = "topo";
        //     save_mesh[child]["fields"].rename_child("dist", "domain_00000" + std::to_string(child) + "_dist");
        // }

        // save_mesh.print();

        // load_mesh.print();

        // // the loaded mesh will be in the multidomain format
        // // (it will be a list containing a single mesh domain)
        // // but the saved mesh is in the single domain format
        // EXPECT_EQ(load_mesh.number_of_children(), save_mesh.number_of_children());
        // NodeConstIterator l_itr = load_mesh.children();
        // NodeConstIterator s_itr = save_mesh.children();
        // while(l_itr.has_next())
        // {
        //     const Node &l_curr = l_itr.next();
        //     const Node &s_curr = s_itr.next();

        //     EXPECT_FALSE(l_curr.diff(s_curr, info));
        // }
    }
}

// TODO we might want "round trip" tests for all the different mesh types
// not round trip though, since not everything will come back unchanged

// TODO units?
