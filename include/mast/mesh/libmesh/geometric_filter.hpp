/*
 * MAST: Multidisciplinary-design Adaptation and Sensitivity Toolkit
 * Copyright (C) 2013-2020  Manav Bhatia and MAST authors
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */


#ifndef __mast__libmesh_geometric_filter_h__
#define __mast__libmesh_geometric_filter_h__


// MAST includes
#include <mast/base/mast_data_types.h>
#include <mast/base/exceptions.hpp>
#include <mast/numerics/utility.hpp>
#include <mast/optimization/design_parameter_vector.hpp>
#include <mast/mesh/libmesh/geometric_filter_augment_send_list.hpp>

// libMesh includes
#include "libmesh/system.h"
#include "libmesh/node.h"
#include "libmesh/elem.h"
#include "libmesh/mesh_base.h"
#include "libmesh/petsc_vector.h"
#ifdef LIBMESH_HAVE_NANOFLANN
#include "libmesh/nanoflann.hpp"
#endif



namespace MAST {
namespace Mesh {
namespace libMeshWrapper {


/*!
 *   Creates a geometric filter for the location-based design variables, for example density and
 *   level-set function parmaters in topology optimization.
 */
class GeometricFilter {
    
public:
    
    /*!
     *   \param sys
     *   \param radius geometric filter radius
     */
    GeometricFilter(libMesh::System         &sys,
                    const real_t            radius):
    _system            (sys),
    _radius            (radius),
    _fe_size           (0.),
    _augment_send_list (nullptr) {
        
        Assert1(radius > 0., radius,
                "geometric filter radius must be greater than 0.");
        
#ifdef LIBMESH_HAVE_NANOFLANN
        _init2();  // KD-tree search using NanoFlann
#else
        _init(); // linear filter search
#endif
        
        // now initialize and attach sendlist to the dofmap
        _augment_send_list =
        new MAST::Mesh::libMeshWrapper::GeometricFilterAugmentSendList(_forward_send_list);
        
        _system.get_dof_map().attach_extra_send_list_object(*_augment_send_list);
        
        // now we tell the function to
        _system.get_dof_map().reinit_send_list(_system.get_mesh());
    }
    
    
    virtual ~GeometricFilter() {
        
        if (_augment_send_list) delete _augment_send_list;
        
        // delete the matrix
        MatDestroy(&_weight_matrix);
    }
    
//    /*!
//     *   computes the filtered output from the provided input.
//     */
//    inline void
//    compute_filtered_values
//    (const MAST::Optimization::DesignParameterVector<real_t> &dvs,
//     const libMesh::NumericVector<real_t>            &input,
//     libMesh::NumericVector<real_t>                  &output,
//     bool                                            close_vec) const {
//
//        Assert2(input.size() == _system.n_dofs(),
//                input.size(), _system.n_dofs(),
//                "Incompatible vector sizes");
//        Assert2(output.size() == _system.n_dofs(),
//                output.size(), _system.n_dofs(),
//                "Incompatible vector sizes");
//
//        output.zero();
//
//        std::vector<real_t> input_vals(input.size(), 0.);
//        input.localize(input_vals);
//
//        const uint_t
//        first_local_dof = _system.get_dof_map().first_dof(_system.comm().rank()),
//        last_local_dof  = _system.get_dof_map().end_dof(_system.comm().rank());
//
//        std::map<uint_t, std::vector<std::pair<uint_t, real_t>>>::const_iterator
//        map_it   = _filter_map.begin(),
//        map_end  = _filter_map.end();
//
//        for ( ; map_it != map_end; map_it++) {
//
//            if (map_it->first >= first_local_dof &&
//                map_it->first <  last_local_dof) {
//
//                std::vector<std::pair<uint_t, real_t>>::const_iterator
//                vec_it  = map_it->second.begin(),
//                vec_end = map_it->second.end();
//
//                for ( ; vec_it != vec_end; vec_it++) {
//                    if (map_it->first >= input.first_local_index() &&
//                        map_it->first <  input.last_local_index()) {
//
//                        if (dvs.is_design_parameter_index(map_it->first))
//                            output.add(map_it->first, input_vals[vec_it->first] * vec_it->second);
//                        else
//                            output.set(map_it->first, input_vals[map_it->first]);
//                    }
//                }
//            }
//        }
//
//        if (close_vec)
//            output.close();
//    }
    
//    /*!
//     *  for large problems it is more efficient to specify only the non-zero entries in the input vector in
//     *  \p nonzero_vals. Here, \p output is expected to be of type SERIAL vector. All ranks in the
//     *  communicator will perform the same operaitons and provide an identical \p output vector.
//     *  If \p close_vector is \p true then \p output.close() will be called in this
//     *  routines, otherwise not.
//     */
//    template <typename ScalarType, typename VecType>
//    inline void
//    compute_filtered_values
//    (const MAST::Optimization::DesignParameterVector<ScalarType> &dvs,
//     const std::map<uint_t, ScalarType>              &nonzero_vals,
//     VecType                                         &output) const {
//
//        Assert2(output.size() == _system.n_dofs(),
//                output.size(), _system.n_dofs(),
//                "Incompatible vector sizes");
//        Assert2(output.type() == libMesh::SERIAL,
//                output.type(), libMesh::SERIAL,
//                "Incompatible vector");
//
//        MAST::Numerics::Utility::setZero(output);
//
//        const uint_t
//        first_local_dof = _system.get_dof_map().first_dof(_system.comm().rank()),
//        last_local_dof  = _system.get_dof_map().end_dof(_system.comm().rank());
//
//        std::map<uint_t, std::vector<std::pair<uint_t, real_t>>>::const_iterator
//        map_it   = _filter_map.begin(),
//        map_end  = _filter_map.end();
//
//        for ( ; map_it != map_end; map_it++) {
//
//            if (map_it->first >= first_local_dof &&
//                map_it->first <  last_local_dof) {
//
//                std::vector<std::pair<uint_t, real_t>>::const_iterator
//                vec_it  = map_it->second.begin(),
//                vec_end = map_it->second.end();
//
//                for ( ; vec_it != vec_end; vec_it++) {
//                    if (nonzero_vals.count(vec_it->first)) {
//
//                        if (dvs.is_design_parameter(map_it->first))
//                            MAST::Numerics::Utility::add
//                            (output, map_it->first, nonzero_vals[vec_it->first] * vec_it->second);
//                        else
//                            MAST::Numerics::Utility::set
//                            (output, map_it->first, nonzero_vals[map_it->first]);
//                    }
//                }
//            }
//        }
//    }
    
    
    inline void
    compute_filtered_values(Vec       input,
                            Vec       output) const {
        
//        Assert2(input.size() == _system.n_dofs(),
//                input.size(), _system.n_dofs(),
//                "Incompatible vector sizes");
//        Assert2(output.size() == _system.n_dofs(),
//                output.size(), _system.n_dofs(),
//                "Incompatible vector sizes");
        
        //MAST::Numerics::Utility::setZero(output);
        
        MatMult(_weight_matrix, input, output);
    }
    
    inline void
    compute_filtered_values(libMesh::NumericVector<real_t>  &input,
                            libMesh::NumericVector<real_t>  &output) const {
        
//        Assert2(input.size() == _system.n_dofs(),
//                input.size(), _system.n_dofs(),
//                "Incompatible vector sizes");
//        Assert2(output.size() == _system.n_dofs(),
//                output.size(), _system.n_dofs(),
//                "Incompatible vector sizes");
        
        //MAST::Numerics::Utility::setZero(output);
        
        this->compute_filtered_values
        (dynamic_cast<libMesh::PetscVector<real_t>&>(input).vec(),
         dynamic_cast<libMesh::PetscVector<real_t>&>(output).vec());
    }


    template <typename ScalarType>
    inline void
    compute_filtered_values(Eigen::Matrix<ScalarType, Eigen::Dynamic, 1>  &input,
                            Eigen::Matrix<ScalarType, Eigen::Dynamic, 1>  &output) const {
        
        Assert2(input.size() == _system.n_dofs(),
                input.size(), _system.n_dofs(),
                "Incompatible vector sizes");
        Assert2(output.size() == _system.n_dofs(),
                output.size(), _system.n_dofs(),
                "Incompatible vector sizes");
        Assert0(_system.comm().size() == 1, "Method only applicable for serial runs");
        
        MAST::Numerics::Utility::setZero(output);
        
        MatMult(_weight_matrix, input, output);
    }

    
    /*!
     *  Applies the reverse map, which is used for sensitivity analysis by first computing the sensitivty wrt filtered coefficients
     *  and then using the columns of the filter coefficient mattix to compute the sensitivity of unfiltered coefficients.
     */
    inline void
    compute_reverse_filtered_values(Vec       input,
                                    Vec       output) const {
        
//        Assert2(input.size() == _system.n_dofs(),
//                input.size(), _system.n_dofs(),
//                "Incompatible vector sizes");
//        Assert2(output.size() == _system.n_dofs(),
//                output.size(), _system.n_dofs(),
//                "Incompatible vector sizes");
        
        //MAST::Numerics::Utility::setZero(output);
        
        MatMultTranspose(_weight_matrix, input, output);
    }

    
    inline void
    compute_reverse_filtered_values(libMesh::NumericVector<real_t>  &input,
                                    libMesh::NumericVector<real_t>  &output) const {
        
//        Assert2(input.size() == _system.n_dofs(),
//                input.size(), _system.n_dofs(),
//                "Incompatible vector sizes");
//        Assert2(output.size() == _system.n_dofs(),
//                output.size(), _system.n_dofs(),
//                "Incompatible vector sizes");
        
        //MAST::Numerics::Utility::setZero(output);

        this->compute_reverse_filtered_values
        (dynamic_cast<libMesh::PetscVector<real_t>&>(input).vec(),
         dynamic_cast<libMesh::PetscVector<real_t>&>(output).vec());
    }

    
    /*!
     *   function identifies if the given element is within the domain of
     *   influence of this specified level set design variable. Currently,
     *   this is identified based on the filter radius, the distance of
     *   element nodes from the specified level set design variable location
     *   and the element sizes.
     */
    inline bool if_elem_in_domain_of_influence(const libMesh::Elem& elem,
                                               const libMesh::Node& node) const {
        
        real_t
        d    = 1.e12; // arbitrarily large value to initialize the search
        
        libMesh::Point
        pt;
        
        // first get the smallest distance from the node to the element nodes
        for (uint_t i=0; i<elem.n_nodes(); i++) {
            pt  = elem.point(i);
            pt -= node;
            
            if (pt.norm() < d)
                d = pt.norm();
        }
        
        // if this distance is outside the domain of influence, then this
        // element is not influenced by the design variable
        return (d>_radius+_fe_size);
    }
    
    
    
    /*!
     *  prints the filter data.
     */
    /*template <typename ScalarType>
    inline void
    print(//const MAST::Optimization::DesignParameterVector<ScalarType> &dvs,
          std::ostream         &o) const {
        
        o << "Filter radius: " << _radius << std::endl;
        
        o
        << std::setw(20) << "Filtered ID"
        << std::setw(20) << "Dependent Vars" << std::endl;
        
        std::map<uint_t, std::vector<std::pair<uint_t, real_t>>>::const_iterator
        map_it   = _filter_map.begin(),
        map_end  = _filter_map.end();
        
        for ( ; map_it != map_end; map_it++) {
            
            o
            << std::setw(20) << map_it->first;
            
            std::vector<std::pair<uint_t, real_t>>::const_iterator
            vec_it  = map_it->second.begin(),
            vec_end = map_it->second.end();
            
            for ( ; vec_it != vec_end; vec_it++) {
                
                //if (dvs.is_design_parameter_index(map_it->first))
                    o
                    << " : " << std::setw(8) << vec_it->first
                    << " (" << std::setw(8) << vec_it->second << " )";
                o << " [ " << _system.get_dof_map().semilocal_index(vec_it->first) << " ] ";
                //else
                //    std::cout
                //    << " : " << std::setw(8) << map_it->first;
            }
            std::cout << std::endl;
        }
    }*/
    
    
private:
    
#ifdef LIBMESH_HAVE_NANOFLANN
    // Nanoflann uses "duck typing" to allow users to define their own adaptors...
    template <uint_t Dim>
    class NanoflannMeshAdaptor
    {
    private:
        // Constant reference to the Mesh we are adapting for use in Nanoflann
        const libMesh::MeshBase & _mesh;
        uint_t                      _n_local_nodes;
        uint_t                      _n_nodes;

    public:
        NanoflannMeshAdaptor (const libMesh::MeshBase & mesh) :
        _mesh           (mesh),
        _n_local_nodes  (mesh.n_local_nodes()),
        _n_nodes        (mesh.n_nodes()) {
            
            nodes.reserve(_n_local_nodes);
            
            libMesh::MeshBase::const_node_iterator
            it   = mesh.local_nodes_begin(),
            end  = mesh.local_nodes_end();
            
            for ( ; it != end; it++) {
                
                nodes.push_back(*it);
                node_id_to_vec_index[(*it)->id()] = nodes.size()-1;
            }
        }
        
        /**
         * libMesh \p Point coordinate type
         */
        typedef real_t coord_t;
        
        /**
         * Must return the number of data points
         */
        inline size_t
        kdtree_get_point_count() const { return _n_local_nodes; }
        
        /**
         * Returns the distance between the vector "p1[0:size-1]"
         * and the data point with index "idx_p2" stored in _mesh
         */
        inline coord_t
        kdtree_distance(const coord_t * p1,
                        const size_t idx_p2,
                        size_t size) const {
            
            Assert2(size == Dim, size, Dim, "Incompatible dimension");
            
            // Construct a libmesh Point object from the input coord_t.  This
            // assumes LIBMESH_DIM==3.
            libMesh::Point point1(p1[0],
                                  size > 1 ? p1[1] : 0.,
                                  size > 2 ? p1[2] : 0.);
            
            // Get the referred-to point from the Mesh
            const libMesh::Point & point2 = _mesh.point(idx_p2);
            
            // Compute Euclidean distance
            return (point1 - point2).norm_sq();
        }
        
        /**
         * Returns the dim'th component of the idx'th point in the class:
         * Since this is inlined and the "dim" argument is typically an immediate value, the
         *  "if's" are actually solved at compile time.
         */
        inline coord_t
        kdtree_get_pt(const size_t idx, int dim) const
        {
            Assert2(dim < (int) Dim, dim, (int) Dim,
                    "Incompatible dimension");
            Assert2(idx < _n_local_nodes, idx, _n_nodes,
                    "Invalid node index");
            Assert1(dim < 3, dim, "Invalid dimension");

            return (*nodes[idx])(dim);
        }
        
        /**
         * Optional bounding-box computation: return false to default to a standard bbox computation loop.
         * Return true if the BBOX was already computed by the class and returned in "bb" so it can be
         * avoided to redo it again. Look at bb.size() to find out the expected dimensionality
         * (e.g. 2 or 3 for point clouds)
         */
        template <class BBOX>
        inline bool kdtree_get_bbox(BBOX & /* bb */) const { return false; }
        
        std::vector<libMesh::Node*> nodes;
        std::map<uint_t, uint_t>    node_id_to_vec_index;
    };
    
    // Declare a type templated on NanoflannMeshAdaptor
    typedef nanoflann::L2_Simple_Adaptor<real_t, NanoflannMeshAdaptor<3> > adatper_t;
    // Declare a KDTree type based on NanoflannMeshAdaptor
    typedef nanoflann::KDTreeSingleIndexAdaptor<adatper_t, NanoflannMeshAdaptor<3>, 3> kd_tree_t;
    
    struct PointData {
        PointData():
        dof_index         (-1),
        local_mesh_index  (-1),
        rank              (-1),
        point             ({0., 0., 0.})
        { }
        
        uint_t              dof_index;
        uint_t              local_mesh_index;
        uint_t              rank;
        std::vector<real_t> point;
    };
    

    inline void _init2() {
        
        libMesh::MeshBase& mesh = _system.get_mesh();
                
        // Loop over nodes to try and detect duplicates.  We use nanoflann
        // for this, inspired by
        // https://gist.github.com/jwpeterson/7a36f9f794df67d51126#file-detect_slit-cc-L65
        // which was inspired by nanoflann example in libMesh source:
        // contrib/nanoflann/examples/pointcloud_adaptor_example.cpp
        
        // Build adaptor and tree objects
        NanoflannMeshAdaptor<3> mesh_adaptor(mesh);
        kd_tree_t kd_tree(3, mesh_adaptor, nanoflann::KDTreeSingleIndexAdaptorParams(/*max leaf=*/10));
        
        // Construct the tree
        kd_tree.buildIndex();
        
        // synchronize with the notes
        // stores the bounding box for each rank
        std::vector<real_t>
        rank_boxes(6, 0.);
        
        // limits of x
        rank_boxes[0] = kd_tree.root_bbox[0].low;
        rank_boxes[1] = kd_tree.root_bbox[0].high;
        // limits of y
        rank_boxes[2] = kd_tree.root_bbox[1].low;
        rank_boxes[3] = kd_tree.root_bbox[1].high;
        // limits of z
        rank_boxes[4] = kd_tree.root_bbox[2].low;
        rank_boxes[5] = kd_tree.root_bbox[2].high;

        // this will modify rank_boxes with such that each block if 6 values is
        // the box limit coordinates from each rank
        mesh.comm().allgather(rank_boxes, true);
        
        Assert2(rank_boxes.size() == mesh.comm().size()*6,
                rank_boxes.size(), mesh.comm().size()*6,
                "Invalid vector dimension after allgather()");
        
        
        real_t
        d_12 = 0.,
        sum  = 0.;

        std::set<uint_t> send_list;
        
        const libMesh::DofMap
        &dof_map = _system.get_dof_map();

        std::vector<PointData>
        point_data;
        
        const uint_t
        size            = mesh.comm().size(),
        rank            = mesh.comm().rank(),
        first_local_dof = dof_map.first_dof(rank),
        last_local_dof  = dof_map.end_dof(rank);

        uint_t
        dof_1,
        dof_2;
        
        std::map<const libMesh::Node*, real_t>
        node_sum;
        
        std::map<uint_t, std::vector<std::pair<uint_t, real_t>>>
        filter_map;
        
        libMesh::MeshBase::const_node_iterator
        node_it      =  mesh.local_nodes_begin(),
        node_end     =  mesh.local_nodes_end();
        
        std::vector<std::vector<const libMesh::Node*>>
        remote_node_dependency(size);
        
        for (; node_it != node_end; node_it++) {

            const libMesh::Node* node = *node_it;

            // check if this node is near the bbox of each rank
            for (uint_t i=0; i<size; i++) {
                
                if (i == rank) continue;
                
                
                if (// within _radius of x-box on rank i
                    (*node)(0) >= rank_boxes[6*i+0]-_radius &&
                    (*node)(0) <= rank_boxes[6*i+1]+_radius &&
                    // within _radius of y-box on rank i
                    (*node)(1) >= rank_boxes[6*i+2]-_radius &&
                    (*node)(1) <= rank_boxes[6*i+3]+_radius &&
                    // within _radius of z-box on rank i
                    (*node)(2) >= rank_boxes[6*i+4]-_radius &&
                    (*node)(2) <= rank_boxes[6*i+5]+_radius) {
                    
                    remote_node_dependency[i].push_back(node);
                }
            }
        }
        
        
        // now communicate with all processors about the filtered nodes
        std::vector<real_t>
        coords_send(size);

        std::vector<std::vector<real_t>>
        coords_recv(size),
        filter_data_node_loc_send(size),
        filter_data_node_loc_recv(size);
        
        std::vector<std::vector<uint_t>>
        // this stores the number of indices that each node in coords_send will depend on
        filter_data_n_indices_send(size),
        filter_data_n_indices_recv(size),
        // this stores the system dof_indices of nodes that the nodes in coord_send will
        // depend on.
        filter_data_dof_index_send(size),
        filter_data_dof_index_recv(size);


        for (uint_t i=0; i<size; i++) {
            
            for (uint_t j=0; j<size; j++) {
                
                if ( i != j) {
                    
                    if (i == rank) {
                        
                        // here, we pack the (x,y,z) coordinates of nodes that the
                        // ith processor wants the jth processor to check
                        coords_send.resize(remote_node_dependency[j].size()*3, 0.);
                        
                        for (uint_t k=0; k<remote_node_dependency[j].size(); k++) {
                            
                            coords_send[k*3 + 0] = (*remote_node_dependency[j][k])(0);
                            coords_send[k*3 + 1] = (*remote_node_dependency[j][k])(1);
                            coords_send[k*3 + 2] = (*remote_node_dependency[j][k])(2);
                        }
                        
                        // send information from i to j
                        mesh.comm().send(j, coords_send);

                        // clear the vector, since we dont need it
                        coords_send.clear();
                    }
                    else if (j == rank) {
                        
                        coords_recv[i].clear();
                        // get information from i
                        mesh.comm().receive(i, coords_recv[i]);
                        
                        Assert1(coords_recv[i].size()%3 == 0,
                                coords_recv[i].size(),
                                "coordinates must be a multiple of 3");
                    }
                }
            }
        }
        
        // now process the data that other processors need from us
        for (uint_t i=0; i<size; i++) {
            
            // nothing to be done if i = rank.
            if (i == rank) continue;
            
            // three coordinates per node, so number of nodes that ith processor
            // asked for is size/3.
            uint_t
            n_nodes = coords_recv[i].size()/3;

            // initialize this to the number of nodes received from the processor
            filter_data_n_indices_send[i].reserve(coords_recv[i].size());
            // we use a conservative estimate of 27 nodes connected to each node
            filter_data_dof_index_send[i].reserve(coords_recv[i].size()*27);
            filter_data_node_loc_send[i].reserve (coords_recv[i].size()*27*3);
            
            // prepare data for rank i
            for (uint_t j=0; j<n_nodes; j++) {
                
                std::vector<std::pair<size_t, real_t>>
                indices_dists;
                nanoflann::RadiusResultSet<real_t, size_t>
                resultSet(_radius*_radius, indices_dists);
                
                kd_tree.findNeighbors(resultSet, &coords_recv[i][j*3], nanoflann::SearchParams());

                // add the information from this search result to the information for this node
                filter_data_n_indices_send[i].push_back(indices_dists.size());
                
                for (unsigned r=0; r<indices_dists.size(); ++r) {
                    
                    const libMesh::Node* nd = mesh_adaptor.nodes[indices_dists[r].first];
                    
                    // location of the node
                    filter_data_node_loc_send[i].push_back((*nd)(0));
                    filter_data_node_loc_send[i].push_back((*nd)(1));
                    filter_data_node_loc_send[i].push_back((*nd)(2));
                    // dof-index of the node
                    filter_data_dof_index_send[i].push_back(nd->dof_number(_system.number(), 0, 0));
                }
            }
        }
        

        // now that all the information is ready, we will communicate it from
        // j to i processor
        for (uint_t i=0; i<size; i++) {
            
            for (uint_t j=0; j<size; j++) {
                
                if (i != j) {
                    
                    if (j == rank) {
                        
                        // send information from j to i
                        mesh.comm().send(i, filter_data_n_indices_send[i]);
                        mesh.comm().send(i, filter_data_dof_index_send[i]);
                        mesh.comm().send(i, filter_data_node_loc_send[i]);
                        
                        // now clear the data since we dont need it any more
                        filter_data_n_indices_send[i].clear();
                        filter_data_dof_index_send[i].clear();
                        filter_data_node_loc_send[i].clear();
                    }
                    else if (i == rank) {
                        
                        // get information from j
                        mesh.comm().receive(j, filter_data_n_indices_recv[j]);
                        mesh.comm().receive(j, filter_data_dof_index_recv[j]);
                        mesh.comm().receive(j, filter_data_node_loc_recv[j]);
                    }
                }
            }
        }

        // now, we search the local nodes and add to it the information from remote couplings
        // For every node in the mesh, search the KDtree and find any
        // nodes at _radius distance from the current
        // node being searched... this will be added to the .
        node_it      =  mesh.local_nodes_begin();
            
        for (; node_it != node_end; node_it++) {
            
            const libMesh::Node* node = *node_it;
            
            dof_1 = node->dof_number(_system.number(), 0, 0);

            // only local dofs are processed.
            if (dof_map.semilocal_index(dof_1)) {
                
                real_t query_pt[3] = {(*node)(0), (*node)(1), (*node)(2)};
                
                std::vector<std::pair<size_t, real_t>>
                indices_dists;
                indices_dists.push_back(std::pair<size_t, real_t>
                                        (mesh_adaptor.node_id_to_vec_index[node->id()], 0.));
                nanoflann::RadiusResultSet<real_t, size_t>
                resultSet(_radius*_radius, indices_dists);
                
                kd_tree.findNeighbors(resultSet, query_pt, nanoflann::SearchParams());
                
                sum       = 0.;
                
                for (unsigned r=0; r<indices_dists.size(); ++r) {
                    
                    d_12 = std::sqrt(indices_dists[r].second);
                    
                    // the distance of this node should be less than or equal to the
                    // specified search radius
                    Assert2(d_12 <= _radius, d_12, _radius,
                            "Node distance must be <= search radius");
                    
                    sum  += _radius - d_12;
                    dof_2 = mesh_adaptor.nodes[indices_dists[r].first]->dof_number(_system.number(), 0, 0);
                    
                    filter_map[dof_1].push_back(std::pair<uint_t, real_t>
                                                (dof_2, _radius - d_12));

                    // add this dof to the local send list
                    if (dof_2 < first_local_dof ||
                        dof_2 >= last_local_dof)
                        send_list.insert(dof_2);
                }
                
                Assert1(sum > 0., sum, "Weight must be > 0.");
                
                // record this for later use
                node_sum[node] = sum;
            }
        }

        
        // also, check the information from remote processors and include them
        // in the map and send list.
        uint_t
        idx            = 0,
        n_remote_nodes = 0;
        
        for (uint_t i=0; i<size; i++) {

            idx = 0;
            
            // number of nodes for which we asked i^th rank to search
            for (uint_t j=0; j<remote_node_dependency[i].size(); j++) {

                const libMesh::Node*
                node = remote_node_dependency[i][j];

                dof_1  = node->dof_number(_system.number(), 0, 0);
                
                // check the number of nodes on the remote node that this
                // node's filtered values will depend on
                n_remote_nodes = filter_data_n_indices_recv[i][j];
                
                // sum from the local nodes for the geometric filter for
                // the present node
                sum = node_sum[node];
                
                // now, we will add this information to the respective map
                for (uint_t k=0; k<n_remote_nodes; k++) {
                    
                    // dof id of this remote node
                    dof_2  = filter_data_dof_index_recv[i][idx];
                    
                    // distance between the present and remote node
                    d_12 =
                    sqrt(pow((*node)(0) - filter_data_node_loc_recv[i][idx*3+0], 2)+
                         pow((*node)(1) - filter_data_node_loc_recv[i][idx*3+1], 2)+
                         pow((*node)(2) - filter_data_node_loc_recv[i][idx*3+2], 2));
                    
                    // contribution to the sum
                    sum += _radius - d_12;
                    
                    // add information to dof_1 node about this remote node
                    filter_map[dof_1].push_back(std::pair<uint_t, real_t>
                                                (dof_2, _radius - d_12));
                    
                    // add this dof to the local send list
                    send_list.insert(dof_2);
                    
                    // increment the counter for the next data
                    idx++;
                }
                
                // update the value of the sum for this node
                node_sum[node] = sum;
            }
        }

        // now normalize the weights with respect to the sum
        // with the coefficients computed for dof_1, divide each coefficient
        // with the sum
        node_it      =  mesh.local_nodes_begin();
            
        for (; node_it != node_end; node_it++) {
            
            // node for which the normalization is being done
            const libMesh::Node* node = *node_it;

            // dof_id for this node
            dof_1 = node->dof_number(_system.number(), 0, 0);

            // filter coefficients for this node
            std::vector<std::pair<uint_t, real_t>>& vec = filter_map[dof_1];
            
            // sum from the local nodes for the geometric filter for
            // the present node
            sum = node_sum[node];
            
            for (uint_t i=0; i<vec.size(); i++) {
                
                vec[i].second /= sum;
                Assert1(vec[i].second <= 1., vec[i].second,
                        "Normalized weight must be <= 1.");
            }
        }

        // use the prepared filtering data to initialize the sparse matrix.
        _init_filter_matrix(filter_map);
        
        
        // now prepare the reverse map. The send list is sorted for later use.
        std::set<uint_t>::const_iterator
        s_it  = send_list.begin(),
        s_end = send_list.end();
        
        _forward_send_list.reserve(send_list.size());
        for ( ; s_it != s_end; s_it++) _forward_send_list.push_back(*s_it);
        
        /*_init_reverse_map(filter_map, _reverse_map);
        
        // compute the largest element size
        libMesh::MeshBase::const_element_iterator
        e_it          = mesh.local_elements_begin(),
        e_end         = mesh.local_elements_end();
        
        for ( ; e_it != e_end; e_it++) {
            const libMesh::Elem* e = *e_it;
            d_12 = e->hmax();
            
            if (_fe_size < d_12)
                _fe_size = d_12;
        }*/

        _system.comm().min(_fe_size);
    }
#endif
    
    
    /*!
     *   initializes the algebraic data structures
     */
    inline void _init() {
        
        libMesh::MeshBase& mesh = _system.get_mesh();
        
        // currently implemented for replicated mesh
        Assert0(mesh.is_replicated(), "Function implemented only for replicated mesh");
        
        std::map<uint_t, std::vector<std::pair<uint_t, real_t>>>
        filter_map;

        // iterate over all nodes to compute the
        libMesh::MeshBase::const_node_iterator
        node_it_1    =  mesh.nodes_begin(),
        node_it_2    =  mesh.nodes_begin(),
        node_end     =  mesh.nodes_end();
        
        libMesh::Point
        d;
        
        real_t
        d_12 = 0.,
        sum  = 0.;
        
        std::set<uint_t> send_list;
        
        const libMesh::DofMap
        &dof_map = _system.get_dof_map();

        const uint_t
        first_local_dof = _system.get_dof_map().first_dof(_system.comm().rank()),
        last_local_dof  = _system.get_dof_map().end_dof(_system.comm().rank());

        uint_t
        dof_1,
        dof_2;
        
        for ( ; node_it_1 != node_end; node_it_1++) {
            
            dof_1 = (*node_it_1)->dof_number(_system.number(), 0, 0);

            // only local dofs are processed.
            if (dof_map.semilocal_index(dof_1)) {
                
                node_it_2 = mesh.nodes_begin();
                sum       = 0.;
                
                for ( ; node_it_2 != node_end; node_it_2++) {
                    
                    // compute the distance between the two nodes
                    d    = (**node_it_1) - (**node_it_2);
                    d_12 = d.norm();
                    
                    // if the nodes is within the filter radius, add it to the map
                    if (d_12 <= _radius) {
                        
                        sum  += _radius - d_12;
                        dof_2 = (*node_it_2)->dof_number(_system.number(), 0, 0);
                        
                        filter_map[dof_1].push_back(std::pair<uint_t, real_t>(dof_2, _radius - d_12));
                        
                        // add this dof to the local send list if it is not a local dof
                        if (dof_2 < first_local_dof ||
                            dof_2 >= last_local_dof)
                            send_list.insert(dof_2);
                    }
                }
                
                Assert1(sum > 0., sum, "Weight must be > 0.");
                
                // with the coefficients computed for dof_1, divide each coefficient
                // with the sum
                std::vector<std::pair<uint_t, real_t>>& vec = filter_map[dof_1];
                for (uint_t i=0; i<vec.size(); i++) {
                    
                    vec[i].second /= sum;
                    Assert1(vec[i].second <= 1., vec[i].second,
                            "Normalized weight must be <= 1.");
                }
            }
        }
        
        // use the prepared filtering data to initialize the sparse matrix.
        _init_filter_matrix(filter_map);

        
        // now prepare the reverse map. The send list is sorted for later use.
        std::set<uint_t>::const_iterator
        s_it  = send_list.begin(),
        s_end = send_list.end();
        
        _forward_send_list.reserve(send_list.size());
        for ( ; s_it != s_end; s_it++) _forward_send_list.push_back(*s_it);
        
        /*_init_reverse_map(_filter_map, _reverse_map);

        // compute the largest element size
        libMesh::MeshBase::const_element_iterator
        e_it          = mesh.elements_begin(),
        e_end         = mesh.elements_end();
        
        for ( ; e_it != e_end; e_it++) {
            const libMesh::Elem* e = *e_it;
            d_12 = e->hmax();
            
            if (_fe_size < d_12)
                _fe_size = d_12;
        }
        */
    }
    
    
    inline void
    _init_reverse_map(const std::map<uint_t, std::vector<std::pair<uint_t, real_t>>> &forward_map,
                      std::map<uint_t, std::vector<std::pair<uint_t, real_t>>>       &reverse_map) {
        
        // now prepare the reverse map
        std::map<uint_t, std::vector<std::pair<uint_t, real_t>>>::const_iterator
        it   =  forward_map.begin(),
        end  =  forward_map.end();
        
        for ( ; it!=end; it++) {
            
            const std::vector<std::pair<uint_t, real_t>>
            &vec = it->second;
            
            for (uint_t i=0; i<vec.size(); i++)
                reverse_map[vec[i].first].push_back(std::pair<uint_t, real_t>(it->first, vec[i].second));
        }
    }
    
    inline void
    _init_filter_matrix
    (const std::map<uint_t, std::vector<std::pair<uint_t, real_t>>>& filter_map) {
        
        const uint_t
        n_vals          = _system.n_dofs(),
        n_local         = _system.get_dof_map().n_dofs_on_processor(_system.processor_id()),
        first_local_dof = _system.get_dof_map().first_dof(_system.comm().rank()),
        last_local_dof  = _system.get_dof_map().end_dof(_system.comm().rank());

        PetscErrorCode   ierr;
     
        std::vector<int>
        n_nz(n_local, 0),
        n_oz(n_local, 0);

        // initialize the data for the sparsity pattern of the matrix
        std::map<uint_t, std::vector<std::pair<uint_t, real_t>>>::const_iterator
        it   =  filter_map.begin(),
        end  =  filter_map.end();
        
        for ( ; it!=end; it++) {
            
            const std::vector<std::pair<uint_t, real_t>>
            &vec = it->second;
            
            for (uint_t i=0; i<vec.size(); i++) {
                uint_t
                dof_id = vec[i].first;
                if (dof_id >= first_local_dof && dof_id <  last_local_dof)
                    n_nz[it->first - first_local_dof]++;
                else
                    n_oz[it->first - first_local_dof]++;
            }
        }

        
        ierr = MatCreate(_system.comm().get(), &_weight_matrix);
        CHKERRABORT(_system.comm().get(), ierr);
        ierr = MatSetSizes(_weight_matrix, n_local, n_local, n_vals, n_vals);
        CHKERRABORT(_system.comm().get(), ierr);

        std::string nm = _system.name() + "_";
        ierr = MatSetOptionsPrefix(_weight_matrix, nm.c_str());
        CHKERRABORT(_system.comm().get(), ierr);

        ierr = MatSetFromOptions(_weight_matrix);
        CHKERRABORT(_system.comm().get(), ierr);
        
        ierr = MatSeqAIJSetPreallocation(_weight_matrix,
                                         n_vals,
                                         (PetscInt*)&n_nz[0]);
        CHKERRABORT(_system.comm().get(), ierr);
        ierr = MatMPIAIJSetPreallocation(_weight_matrix,
                                         0,
                                         (PetscInt*)&n_nz[0],
                                         0,
                                         (PetscInt*)&n_oz[0]);
        CHKERRABORT(_system.comm().get(), ierr);
        ierr = MatSetOption(_weight_matrix,
                            MAT_NEW_NONZERO_ALLOCATION_ERR,
                            PETSC_TRUE);
        CHKERRABORT(_system.comm().get(), ierr);

        // now populate the matrix
        it   =  filter_map.begin();
        end  =  filter_map.end();
        
        for ( ; it!=end; it++) {
            
            const std::vector<std::pair<uint_t, real_t>>
            &vec = it->second;
            
            for (uint_t i=0; i<vec.size(); i++) {
                MatSetValue(_weight_matrix,
                            it->first,
                            vec[i].first,
                            vec[i].second,
                            INSERT_VALUES);
            }
        }
        
        MatAssemblyBegin(_weight_matrix, MAT_FINAL_ASSEMBLY);
        CHKERRABORT(_system.comm().get(), ierr);
        MatAssemblyEnd(_weight_matrix, MAT_FINAL_ASSEMBLY);
        CHKERRABORT(_system.comm().get(), ierr);
    }
    
    
    /*!
     *   system on which the level set discrete function is defined
     */
    libMesh::System& _system;
    
    /*!
     *   radius of the filter.
     */
    real_t _radius;
    
    
    /*!
     *   largest element size in the level set mesh
     */
    real_t _fe_size;
    
    /*!
     *  pointer to an object that appends the sendlist for dofmap to localize the dofs needed for local
     *  computations.
     */
    MAST::Mesh::libMeshWrapper::GeometricFilterAugmentSendList *_augment_send_list;

    /*!
     *   Algebraic relation between filtered level set values and the
     *   design variables \f$ \tilde{\phi}_i = B_{ij} \phi_j \f$
     */
    //std::map<uint_t, std::vector<std::pair<uint_t, real_t>>> _filter_map;
    
    /*!
     * this map stores the columns of the matrix, which is required for sensitivity analysis
     */
    //std::map<uint_t, std::vector<std::pair<uint_t, real_t>>> _reverse_map;
    
    /*!
     *   vector of dof ids that the current processor depends on.
     */
    std::vector<uint_t> _forward_send_list;
    
    /*!
     * Sparse Matrix object that stores the filtering matrix. This is used for both both forward and reverse operations
     */
    Mat _weight_matrix;
};


} // namespace libMeshWrapper
} // namespace Mesh
} // namespace MAST


#endif // __mast__libmesh_geometric_filter_h__
