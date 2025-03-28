#include "Profiler.h"

#include <string.h>

#include "ConVar.h"
#include "Engine.h"

ProfilerProfile g_profCurrentProfile(true);

ProfilerProfile::ProfilerProfile(bool manualStartViaMain) : root("Root", VPROF_BUDGETGROUP_ROOT, NULL) {
    this->bManualStartViaMain = manualStartViaMain;

    this->iEnabled = 0;
    this->bEnableScheduled = false;
    this->bAtRoot = true;
    this->curNode = &this->root;

    this->iNumGroups = 0;
    for(size_t i = 0; i < VPROF_MAX_NUM_BUDGETGROUPS; i++) {
        this->groups[i].name = NULL;
    }

    this->iNumNodes = 0;

    // create all groups in predefined order
    this->groupNameToID(
        VPROF_BUDGETGROUP_ROOT);  // NOTE: the root group must always be the first group to be created here
    this->groupNameToID(VPROF_BUDGETGROUP_SLEEP);
    this->groupNameToID(VPROF_BUDGETGROUP_WNDPROC);
    this->groupNameToID(VPROF_BUDGETGROUP_UPDATE);
    this->groupNameToID(VPROF_BUDGETGROUP_DRAW);
    this->groupNameToID(VPROF_BUDGETGROUP_DRAW_SWAPBUFFERS);
}

double ProfilerProfile::sumTimes(int groupID) { return this->sumTimes(&this->root, groupID); }

double ProfilerProfile::sumTimes(ProfilerNode *node, int groupID) {
    if(node == NULL) return 0.0;

    double sum = 0.0;

    ProfilerNode *sibling = node;
    while(sibling != NULL) {
        if(sibling->iGroupID == groupID) {
            if(sibling == &this->root)
                return (this->root.child != NULL
                            ? this->root.child->fTimeLastFrame
                            : this->root.fTimeLastFrame);  // special case: early return for the root group (total
                                                           // duration of the entire frame)
            else
                return (sum + sibling->fTimeLastFrame);
        } else {
            if(sibling->child != NULL) sum += this->sumTimes(sibling->child, groupID);
        }

        sibling = sibling->sibling;
    }

    return sum;
}

int ProfilerProfile::groupNameToID(const char *group) {
    if(this->iNumGroups >= VPROF_MAX_NUM_BUDGETGROUPS) {
        engine->showMessageErrorFatal("Engine", "Increase VPROF_MAX_NUM_BUDGETGROUPS");
        engine->shutdown();
        return -1;
    }

    for(int i = 0; i < this->iNumGroups; i++) {
        if(this->groups[i].name != NULL && group != NULL && strcmp(this->groups[i].name, group) == 0) return i;
    }

    const int newID = this->iNumGroups;
    this->groups[this->iNumGroups++].name = group;
    return newID;
}

int ProfilerNode::s_iNodeCounter = 0;

ProfilerNode::ProfilerNode() : ProfilerNode(NULL, NULL, NULL) {}

ProfilerNode::ProfilerNode(const char *name, const char *group, ProfilerNode *parent) {
    this->constructor(name, group, parent);
}

void ProfilerNode::constructor(const char *name, const char *group, ProfilerNode *parent) {
    this->name = name;
    this->parent = parent;
    this->child = NULL;
    this->sibling = NULL;

    this->iNumRecursions = 0;
    this->fTime = 0.0;
    this->fTimeCurrentFrame = 0.0;
    this->fTimeLastFrame = 0.0;

    this->iGroupID = (s_iNodeCounter++ > 0 ? g_profCurrentProfile.groupNameToID(group) : 0);
}

void ProfilerNode::enterScope() {
    if(this->iNumRecursions++ == 0) {
        this->fTime = engine->getTimeReal();
    }
}

bool ProfilerNode::exitScope() {
    if(--this->iNumRecursions == 0) {
        this->fTime = engine->getTimeReal() - this->fTime;
        this->fTimeCurrentFrame = this->fTime;
    }

    return (this->iNumRecursions == 0);
}

ProfilerNode *ProfilerNode::getSubNode(const char *name, const char *group) {
    // find existing node
    ProfilerNode *child = this->child;
    while(child != NULL) {
        if(child->name == name)  // NOTE: pointer comparison
            return child;

        child = child->sibling;
    }

    // "add" new node
    if(g_profCurrentProfile.iNumNodes >= VPROF_MAX_NUM_NODES) {
        engine->showMessageErrorFatal("Engine", "Increase VPROF_MAX_NUM_NODES");
        engine->shutdown();
        return NULL;
    }

    ProfilerNode *node = &(g_profCurrentProfile.nodes[g_profCurrentProfile.iNumNodes++]);
    node->constructor(name, group, this);
    node->sibling = this->child;
    this->child = node;

    return node;
}
