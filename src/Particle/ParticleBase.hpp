/***************************************************************************
 *
 * The IPPL Framework
 *
 * This program was prepared by PSI.
 * All rights in the program are reserved by PSI.
 * Neither PSI nor the author(s)
 * makes any warranty, express or implied, or assumes any liability or
 * responsibility for the use of this software
 *
 * Visit www.amas.web.psi for more details
 *
 ***************************************************************************/

// #include "Particle/Kokkos_ParticleBase.h"
// #include "Particle/ParticleLayout.h"
// #include "Particle/ParticleAttrib.h"
// #include "Message/Message.h"
// #include "Message/Communicate.h"
// #include "Utility/Inform.h"
// #include "Utility/PAssert.h"
// #include "Utility/IpplInfo.h"
// #include "Utility/IpplStats.h"
// #include "Utility/IpplException.h"
// #include <algorithm>

namespace ippl {

    template<class PLayout>
    ParticleBase<PLayout>::ParticleBase()
    : ParticleBase(nullptr)
    { }

    template<class PLayout>
    ParticleBase<PLayout>::ParticleBase(std::shared_ptr<PLayout>& layout)
    : ParticleBase()
    {
        initialize(layout);
    }

    template<class PLayout>
    ParticleBase<PLayout>::ParticleBase(std::shared_ptr<PLayout>&& layout)
    : layout_m(std::move(layout))
    , totalNum_m(0)
    , localNum_m(0)
    , destroyNum_m(0)
    , attributes_m(0)
    , nextID_m(Ippl::Comm->myNode())
    , numNodes_m(Ippl::Comm->getNodes())
    {
        addAttribute(R);
        addAttribute(ID);
    }


    template<class PLayout>
    void ParticleBase<PLayout>::addAttribute(ParticleAttribBase& pa)
    {
        attributes_m.push_back(&pa);
    }

    template<class PLayout>
    void ParticleBase<PLayout>::initialize(std::shared_ptr<PLayout>& layout)
    {
        PAssert(layout_m == nullptr);

        // save the layout, and perform setup tasks
        layout_m = std::move(layout);
    }


    template<class PLayout>
    void ParticleBase<PLayout>::create(size_t nLocal)
    {
//     // make sure we've been initialized
//     PAssert(Layout != 0);

        for (attribute_iterator it = attributes_m.begin();
             it != attributes_m.end(); ++it) {
            (*it)->create(nLocal);
        }

        // set the unique ID value for these new particles
        Kokkos::parallel_for("ParticleBase<PLayout>::create(size_t)",
                             Kokkos::RangePolicy(localNum_m, nLocal),
                             KOKKOS_CLASS_LAMBDA(const size_t i) {
                                 ID(i) = this->nextID_m + this->numNodes_m * i;
                             });
        nextID_m += numNodes_m * (nLocal - localNum_m);

        // remember that we're creating these new particles
        localNum_m += nLocal;
    }

    template<class PLayout>
    void ParticleBase<PLayout>::createWithID(index_type id) {
//         // make sure we've been initialized
//         PAssert(Layout != 0);

        // temporary change
        index_type tmpNextID = nextID_m;
        nextID_m = id;
        numNodes_m = 0;

        create(1);

        nextID_m = tmpNextID;
        numNodes_m = Ippl::Comm->getNodes();
    }

    template<class PLayout>
    void ParticleBase<PLayout>::globalCreate(size_t nTotal) {
//         // make sure we've been initialized
//         PAssert(Layout != 0);

        // Compute the number of particles local to each processor
        size_t nLocal = nTotal / numNodes_m;

        const size_t rank = Ippl::Comm->myNode();

        size_t rest = nTotal - nLocal * rank;
        if (rank < rest)
            ++nLocal;

        create(nLocal);
    }


//
//

//
//
//     /////////////////////////////////////////////////////////////////////
//     // delete M particles, starting with the Ith particle.  If the last argument
//     // is true, the destroy will be done immediately, otherwise the request
//     // will be cached.
//     template<class PLayout>
//     void ParticleBase<PLayout>::destroy(size_t M, size_t I, bool doNow) {
//
//     // make sure we've been initialized
//     PAssert(Layout != 0);
//
//     if (M > 0) {
//         if (doNow) {
//         // find out if we are using optimized destroy method
//         bool optDestroy = getUpdateFlag(PLayout::OPTDESTROY);
//         // loop over attributes and carry out the destroy request
//         attrib_container_t::iterator abeg, aend = AttribList.end();
//         for (abeg = AttribList.begin(); abeg != aend; ++abeg)
//             (*abeg)->destroy(M,I,optDestroy);
//         LocalNum -= M;
//         }
//         else {
//         // add this group of particle indices to our list of items to destroy
//         std::pair<size_t,size_t> destroyEvent(I,M);
//         DestroyList.push_back(destroyEvent);
//         DestroyNum += M;
//         }
//
//         // remember we have this many more items to destroy (or have destroyed)
//         ADDIPPLSTAT(incParticlesDestroyed,M);
//     }
//     }
//
//
//     /////////////////////////////////////////////////////////////////////
//     // Update the particle object after a timestep.  This routine will change
//     // our local, total, create particle counts properly.
//     template<class PLayout>
//     void ParticleBase<PLayout>::update() {
//
//
//
//     // make sure we've been initialized
//     PAssert(Layout != 0);
//
//     // ask the layout manager to update our atoms, etc.
//     Layout->update(*this);
//     INCIPPLSTAT(incParticleUpdates);
//     }
//
//
//     /////////////////////////////////////////////////////////////////////
//     // Update the particle object after a timestep.  This routine will change
//     // our local, total, create particle counts properly.
//     template<class PLayout>
//     void ParticleBase<PLayout>::update(const ParticleAttrib<char>& canSwap) {
//
//
//
//     // make sure we've been initialized
//     PAssert(Layout != 0);
//
//     // ask the layout manager to update our atoms, etc.
//     Layout->update(*this, &canSwap);
//     INCIPPLSTAT(incParticleUpdates);
//     }
//
//
//     // Actually perform the delete atoms action for all the attributes; the
//     // calls to destroy() only stored a list of what to do.  This actually
//     // does it.  This should in most cases only be called by the layout manager.
//     template<class PLayout>
//     void ParticleBase<PLayout>::performDestroy(bool updateLocalNum) {
//
//
//
//     // make sure we've been initialized
//     PAssert(Layout != 0);
//
//     // nothing to do if destroy list is empty
//     if (DestroyList.empty()) return;
//
//     // before processing the list, we should make sure it is sorted
//     bool isSorted = true;
//     typedef std::vector< std::pair<size_t,size_t> > dlist_t;
//     dlist_t::const_iterator curr = DestroyList.begin();
//     const dlist_t::const_iterator last = DestroyList.end();
//     dlist_t::const_iterator next = curr + 1;
//     while (next != last && isSorted) {
//         if (*next++ < *curr++) isSorted = false;
//     }
//     if (!isSorted)
//         std::sort(DestroyList.begin(),DestroyList.end());
//
//     // find out if we are using optimized destroy method
//     bool optDestroy = getUpdateFlag(PLayout::OPTDESTROY);
//
//     // loop over attributes and process destroy list
//     attrib_container_t::iterator abeg, aend = AttribList.end();
//     for (abeg = AttribList.begin(); abeg != aend; ++abeg)
//         (*abeg)->destroy(DestroyList,optDestroy);
//
//     if (updateLocalNum) {
//         for (curr = DestroyList.begin(); curr != last; ++ curr) {
//             LocalNum -= curr->second;
//         }
//     }
//
//     // clear destroy list and update destroy num counter
//     DestroyList.erase(DestroyList.begin(),DestroyList.end());
//     DestroyNum = 0;
//     }
//
//
//     /////////////////////////////////////////////////////////////////////
//     // delete M ghost particles, starting with the Ith particle.
//     // This is done immediately.
//     template<class PLayout>
//     void ParticleBase<PLayout>::ghostDestroy(size_t M, size_t I) {
//
//
//
//     // make sure we've been initialized
//     PAssert(Layout != 0);
//
//     if (M > 0) {
//         // delete the data from the attribute containers
//         size_t dnum = 0;
//         attrib_container_t::iterator abeg = AttribList.begin();
//         attrib_container_t::iterator aend = AttribList.end();
//         for ( ; abeg != aend; ++abeg )
//         dnum = (*abeg)->ghostDestroy(M, I);
//         GhostNum -= dnum;
//     }
//     }
//
//
//     /////////////////////////////////////////////////////////////////////
//     // Put the data for M particles starting from local index I in a Message.
//     // Return the number of particles put in the Message.  This is for building
//     // ghost particle interaction lists.
//     template<class PLayout>
//     size_t
//     ParticleBase<PLayout>::ghostPutMessage(Message &msg, size_t M, size_t I) {
//
//     // make sure we've been initialized
//     PAssert(Layout != 0);
//
//     // put into message the number of items in the message
//     if (I >= R.size()) {
//         // we're putting in ghost particles ...
//         if ((I + M) > (R.size() + GhostNum))
//         M = (R.size() + GhostNum) - I;
//     } else {
//         // we're putting in local particles ...
//         if ((I + M) > R.size())
//         M = R.size() - I;
//     }
//     msg.put(M);
//
//     // go through all the attributes and put their data in the message
//     if (M > 0) {
//         attrib_container_t::iterator abeg = AttribList.begin();
//         attrib_container_t::iterator aend = AttribList.end();
//         for ( ; abeg != aend; abeg++ )
//         (*abeg)->ghostPutMessage(msg, M, I);
//     }
//
//     return M;
//     }
//
//
//     /////////////////////////////////////////////////////////////////////
//     // put the data for particles on a list into a Message, given list of indices
//     // Return the number of particles put in the Message.  This is for building
//     // ghost particle interaction lists.
//     template<class PLayout>
//     size_t
//     ParticleBase<PLayout>::ghostPutMessage(Message &msg,
//                                         const std::vector<size_t>& pl) {
//
//     // make sure we've been initialized
//     PAssert(Layout != 0);
//
//     std::vector<size_t>::size_type M = pl.size();
//     msg.put(M);
//
//     // go through all the attributes and put their data in the message
//     if (M > 0) {
//         attrib_container_t::iterator abeg = AttribList.begin();
//         attrib_container_t::iterator aend = AttribList.end();
//         for ( ; abeg != aend; ++abeg )
//         (*abeg)->ghostPutMessage(msg, pl);
//     }
//
//     return M;
//     }
//
//
//     /////////////////////////////////////////////////////////////////////
//     // retrieve particles from the given message and sending node and store them
//     template<class PLayout>
//     size_t
//     ParticleBase<PLayout>::ghostGetMessage(Message& msg, int /*node*/) {
//
//
//
//     // make sure we've been initialized
//     PAssert(Layout != 0);
//
//     // get the number of items in the message
//     size_t numitems;
//     msg.get(numitems);
//     GhostNum += numitems;
//
//     // go through all the attributes and get their data from the message
//     if (numitems > 0) {
//         attrib_container_t::iterator abeg = AttribList.begin();
//         attrib_container_t::iterator aend = AttribList.end();
//         for ( ; abeg != aend; abeg++ )
//         (*abeg)->ghostGetMessage(msg, numitems);
//     }
//
//     return numitems;
//     }
//
//     template<class PLayout>
//     size_t
//     ParticleBase<PLayout>::ghostGetSingleMessage(Message& msg, int /*node*/) {
//
//     // make sure we've been initialized
//     PAssert(Layout != 0);
//
//     // get the number of items in the message
//     size_t numitems=1;
//     GhostNum += numitems;
//
//     // go through all the attributes and get their data from the message
//     if (numitems > 0) {
//         attrib_container_t::iterator abeg = AttribList.begin();
//         attrib_container_t::iterator aend = AttribList.end();
//         for ( ; abeg != aend; abeg++ )
//         (*abeg)->ghostGetMessage(msg, numitems);
//     }
//
//     return numitems;
//     }
//
//     /////////////////////////////////////////////////////////////////////
//     // Apply the given sort-list to all the attributes.  The sort-list
//     // may be temporarily modified, thus it must be passed by non-const ref.
//     template<class PLayout>
//     void ParticleBase<PLayout>::sort(SortList_t &sortlist) {
//     attrib_container_t::iterator abeg = AttribList.begin();
//     attrib_container_t::iterator aend = AttribList.end();
//     for ( ; abeg != aend; ++abeg )
//         (*abeg)->sort(sortlist);
//     }
//
//
//     /////////////////////////////////////////////////////////////////////
//     // print it out
//     template<class PLayout>
//     std::ostream& operator<<(std::ostream& out, const ParticleBase<PLayout>& P) {
//
//
//     out << "Particle object contents:";
//     out << "\n  Total particles: " << P.getTotalNum();
//     out << "\n  Local particles: " << P.getLocalNum();
//     out << "\n  Attributes (including R and ID): " << P.numAttributes();
//     out << "\n  Layout = " << P.getLayout();
//     return out;
//     }
//
//
//     /////////////////////////////////////////////////////////////////////
//     // print out debugging information
//     template<class PLayout>
//     void ParticleBase<PLayout>::printDebug(Inform& o) {
//
//     o << "PBase: total = " << getTotalNum() << ", local = " << getLocalNum();
//     o << ", attributes = " << AttribList.size() << endl;
//     for (attrib_container_t::size_type i=0; i < AttribList.size(); ++i) {
//         o << "    ";
//         AttribList[i]->printDebug(o);
//         o << endl;
//     }
//     o << "    ";
//     Layout->printDebug(o);
//     }
}
