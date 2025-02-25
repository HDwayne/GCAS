#ifndef IOC_REGALLOC_HPP
#define IOC_REGALLOC_HPP

#include <list>
#include <map>
using namespace std;

#include "Inst.hpp"
#include "Quad.hpp"

class StackMapper {
public:
	StackMapper();
	void add(Quad::reg_t reg);
	int32_t offsetOf(Quad::reg_t reg);
	void markGlobal();
	bool isGlobal(Quad::reg_t reg);
	void rewind();
private:
	int32_t _offset, _global;
	map<Quad::reg_t, int32_t> _offsets;
};

class RegAlloc {
public:
	RegAlloc(StackMapper& mapper, list<Inst>& insts);
	void process(Inst inst);
	void complete();
private:
	void processRead(Param& param);
	void processWrite(Param& param);
	Quad::reg_t allocate(Quad::reg_t reg);
	void spill(Quad::reg_t reg);
	void free(Quad::reg_t reg);
	void store(Quad::reg_t reg);
	void load(Quad::reg_t reg);
	bool isVar(Quad::reg_t reg) const;

	map<Quad::reg_t, Quad::reg_t> _map; // reg (virt) -> reg (phys) => mapping
	list<Quad::reg_t> _written; // reg (virt) written in the BB
	list<Quad::reg_t> _avail; // reg (phys) available for allocation
	StackMapper& _mapper; // reg -> offset in stack mapping (stack) 
	list<Inst>& _insts; // instructions to add the generated code to 
	list<Quad::reg_t> _fried; // reg (phys) to free
};

#endif	// IOC_REGALLOC_HPP
