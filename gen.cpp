#include "AST.hpp"
#include "Quad.hpp"

#include <assert.h>

const Quad::lab_t
    base_call = 10000,
    field_get_call = 10000,    // R0=expr, R1=high bit, R2=low bit
    field_set_call = 100001;   // R0=expr, R1=high bit, R2=low bit, R2=assigned value

///
/// Génération d'une constante
Quad::reg_t ConstExpr::gen(QuadProgram& prog) {
    auto r = prog.newReg();
    prog.emit(Quad::seti(r, _val));
    return r;
}

///
/// Génération d'un accès mémoire
Quad::reg_t MemExpr::gen(QuadProgram& prog) {
    switch(_dec->type()) {

    case Declaration::CST: {
            auto r = prog.newReg();
            prog.emit(Quad::seti(r, static_cast<ConstDecl *>(_dec)->value()));
            return r;
        }

    case Declaration::VAR:
        return prog.regFor(static_cast<VarDecl *>(_dec)->name());

    case Declaration::REG: {
            // Read from memory address of the register
            auto ra = prog.newReg(); // Register to hold the address
            auto rd = prog.newReg(); // Register to hold the data
            prog.emit(Quad::seti(ra, static_cast<RegDecl *>(_dec)->address()));
            prog.emit(Quad::load(rd, ra));
            return rd;
        }

    default:
        assert(false);
        return 0;
    }
}

///
/// Génération d'une opération unaire
Quad::reg_t UnopExpr::gen(QuadProgram& prog) {
    auto ro = _arg->gen(prog);
    auto r = prog.newReg();
    switch(_op) {
    case NEG:
        prog.emit(Quad::neg(r, ro));
        break;
    case INV:
        prog.emit(Quad::inv(r, ro));
        break;
    default:
        assert(false);
        break;
    }
    return r;
}

///
/// Génération d'une opération binaire
Quad::reg_t BinopExpr::gen(QuadProgram& prog) {
    auto r1 = _arg1->gen(prog);
    auto r2 = _arg2->gen(prog);
    auto rd = prog.newReg();
    switch(_op) {
    case ADD:
        prog.emit(Quad::add(rd, r1, r2));
        break;
    case SUB:
        prog.emit(Quad::sub(rd, r1, r2));
        break;
    case MUL:
        prog.emit(Quad::mul(rd, r1, r2));
        break;
    case DIV:
        prog.emit(Quad::div(rd, r1, r2));
        break;
    case MOD:
        prog.emit(Quad::mod(rd, r1, r2));
        break;
    case BIT_AND:
        prog.emit(Quad::and_(rd, r1, r2));
        break;
    case BIT_OR:
        prog.emit(Quad::or_(rd, r1, r2));
        break;
    case XOR:
        prog.emit(Quad::xor_(rd, r1, r2));
        break;
    case SHL:
        prog.emit(Quad::shl(rd, r1, r2));
        break;
    case SHR:
        prog.emit(Quad::shr(rd, r1, r2));
        break;
    case ROL:
        prog.emit(Quad::rol(rd, r1, r2));
        break;
    case ROR:
        prog.emit(Quad::ror(rd, r1, r2));
        break;
    default:
        assert(false);
        break;
    }
    return rd;
}

///
/// Génération d'une expression de champ de bits
Quad::reg_t BitFieldExpr::gen(QuadProgram& prog) {
    // Generate registers for the expression and boundaries
    auto expr_reg = _expr->gen(prog);  // The register for the main expression
    auto hi_reg = _hi->gen(prog);      // The high bit register
    auto lo_reg = _lo->gen(prog);      // The low bit register

    // Allocate a new register for the final result
    auto result_reg = prog.newReg();

    // Check if high and low bounds are constant
    auto hi_val_opt = _hi->eval();
    auto lo_val_opt = _lo->eval();

    // If both hi and lo are constant, we can optimize the mask calculation
    if (hi_val_opt && lo_val_opt) {
        int hi_val = *hi_val_opt;
        int lo_val = *lo_val_opt;

        if (hi_val == lo_val) {
            // Special case: Extract a single bit
            auto shifted_reg = prog.newReg();
            prog.emit(Quad::shr(shifted_reg, expr_reg, lo_reg));
            auto mask_reg = prog.newReg();
            prog.emit(Quad::seti(mask_reg, 1));
            prog.emit(Quad::and_(result_reg, shifted_reg, mask_reg));
        } else {
            // General case: Extract multiple bits
            int mask_val = (1 << (hi_val - lo_val + 1)) - 1;
            auto shifted_reg = prog.newReg();
            auto mask_reg = prog.newReg();

            // Shift expression right by lo to bring target bits to the LSB
            prog.emit(Quad::shr(shifted_reg, expr_reg, lo_reg));

            // Apply the mask
            prog.emit(Quad::seti(mask_reg, mask_val));
            prog.emit(Quad::and_(result_reg, shifted_reg, mask_reg));
        }
    } else {
        // General case: hi or lo is not constant
        auto mask_reg = prog.newReg();

        // Compute the mask based on (1 << (hi - lo + 1)) - 1
        auto diff_reg = prog.newReg();
        prog.emit(Quad::sub(diff_reg, hi_reg, lo_reg));
        auto one_reg = prog.newReg();
        prog.emit(Quad::seti(one_reg, 1));
        prog.emit(Quad::add(diff_reg, diff_reg, one_reg));  // hi - lo + 1
        prog.emit(Quad::shl(mask_reg, one_reg, diff_reg));
        prog.emit(Quad::sub(mask_reg, mask_reg, one_reg));  // Mask value

        // Shift expression by lo
        auto shifted_reg = prog.newReg();
        prog.emit(Quad::shr(shifted_reg, expr_reg, lo_reg));

        // Apply mask to the shifted expression
        prog.emit(Quad::and_(result_reg, shifted_reg, mask_reg));
    }

    return result_reg;
}

///
/// Génération d'une comparaison
void CompCond::gen(Quad::lab_t lab_true, Quad::lab_t lab_false, QuadProgram& prog) const {
    auto a1 = _arg1->gen(prog);
    auto a2 = _arg2->gen(prog);
    switch(_comp) {
    case EQ: prog.emit(Quad::goto_eq(lab_true, a1, a2)); break;
    case NE: prog.emit(Quad::goto_ne(lab_true, a1, a2)); break;
    case LT: prog.emit(Quad::goto_lt(lab_true, a1, a2)); break;
    case LE: prog.emit(Quad::goto_le(lab_true, a1, a2)); break;
    case GT: prog.emit(Quad::goto_gt(lab_true, a1, a2)); break;
    case GE: prog.emit(Quad::goto_ge(lab_true, a1, a2)); break;
    default:
        assert(false);
        break;
    }
    prog.emit(Quad::goto_(lab_false));
}

///
/// Génération d'une négation de condition
void NotCond::gen(Quad::lab_t lab_true, Quad::lab_t lab_false, QuadProgram& prog) const {
    _cond->gen(lab_false, lab_true, prog);
}

///
/// Génération d'un ET logique
void AndCond::gen(Quad::lab_t lab_true, Quad::lab_t lab_false, QuadProgram& prog) const {
    auto lab_mid = prog.newLab();
    _cond1->gen(lab_mid, lab_false, prog);
    prog.emit(Quad::lab(lab_mid));
    _cond2->gen(lab_true, lab_false, prog);
}

///
/// Génération d'un OU logique
void OrCond::gen(Quad::lab_t lab_true, Quad::lab_t lab_false, QuadProgram& prog) const {
    auto lab_mid = prog.newLab();
    _cond1->gen(lab_true, lab_mid, prog);
    prog.emit(Quad::lab(lab_mid));
    _cond2->gen(lab_true, lab_false, prog);
}

///
/// Génération d'une instruction NOP (vide)
void NOPStatement::gen(AutoDecl& automaton, QuadProgram& prog) const {
    // Rien à générer pour NOP
}

///
/// Génération d'une séquence d'instructions
void SeqStatement::gen(AutoDecl& automaton, QuadProgram& prog) const {
    _stmt1->gen(automaton, prog);
    _stmt2->gen(automaton, prog);
}

///
/// Génération d'une instruction if
void IfStatement::gen(AutoDecl& automaton, QuadProgram& prog) const {
    prog.comment(pos);
    auto lab_true = prog.newLab();
    auto lab_false = prog.newLab();
    auto lab_end = prog.newLab();
    _cond->gen(lab_true, lab_false, prog);
    prog.emit(Quad::lab(lab_true));
    _stmt1->gen(automaton, prog);
    prog.emit(Quad::goto_(lab_end));
    prog.emit(Quad::lab(lab_false));
    if(_stmt2 != nullptr)
        _stmt2->gen(automaton, prog);
    prog.emit(Quad::lab(lab_end));
}

///
/// Génération d'une instruction d'affectation
void SetStatement::gen(AutoDecl& automaton, QuadProgram& prog) const {
    prog.comment(pos);
    auto r = _expr->gen(prog);
    switch(_dec->type()) {
    case Declaration::VAR:
        prog.emit(Quad::set(prog.regFor(static_cast<VarDecl *>(_dec)->name()), r));
        break;
    case Declaration::REG: {
            auto ra = prog.newReg();
            prog.emit(Quad::seti(ra, static_cast<RegDecl *>(_dec)->address()));
            prog.emit(Quad::store(ra, r));
        }
        break;
    default:
        assert(false);
        break;
    }
}

///
/// Génération d'une affectation de champ de bits
void SetFieldStatement::gen(AutoDecl& automaton, QuadProgram& prog) const {
    prog.comment(pos);

    // Generate registers for high, low, and value expressions
    auto hi_reg = _hi->gen(prog);
    auto lo_reg = _lo->gen(prog);
    auto value_reg = _expr->gen(prog);

    // Declare a register to hold the variable or register value of E
    Quad::reg_t e_reg;
    Quad::reg_t addr_reg;

    // Initialize e_reg based on whether _dec is a variable or register
    switch (_dec->type()) {
        case Declaration::VAR:
            e_reg = prog.regFor(static_cast<VarDecl *>(_dec)->name());
            break;

        case Declaration::REG:
            addr_reg = prog.newReg();
            prog.emit(Quad::seti(addr_reg, static_cast<RegDecl *>(_dec)->address()));
            e_reg = prog.newReg();
            prog.emit(Quad::load(e_reg, addr_reg));
            break;

        default:
            assert(false); // Shouldn’t happen
            return;
    }

    // Try to evaluate high, low, and value if they are constants
    auto hi_val_opt = _hi->eval();
    auto lo_val_opt = _lo->eval();
    auto value_val_opt = _expr->eval();

    if (hi_val_opt && lo_val_opt && value_val_opt) {
        // If hi, lo, and value are constants
        int hi_val = hi_val_opt.value();
        int lo_val = lo_val_opt.value();
        int value_val = value_val_opt.value();

        if (hi_val == lo_val) {
            // If we're setting a single bit
            int bit_pos = lo_val;
            auto bit_mask = prog.newReg();

            if (value_val == 1) {
                // bit_mask = 1 << bit_pos
                prog.emit(Quad::seti(bit_mask, 1));
                auto shift_reg = prog.newReg();
                prog.emit(Quad::seti(shift_reg, bit_pos));
                prog.emit(Quad::shl(bit_mask, bit_mask, shift_reg));

                // E = E | bit_mask
                prog.emit(Quad::or_(e_reg, e_reg, bit_mask));
            } else if (value_val == 0) {
                // bit_mask = ~(1 << bit_pos)
                prog.emit(Quad::seti(bit_mask, 1));
                auto shift_reg = prog.newReg();
                prog.emit(Quad::seti(shift_reg, bit_pos));
                prog.emit(Quad::shl(bit_mask, bit_mask, shift_reg));
                prog.emit(Quad::inv(bit_mask, bit_mask));

                // E = E & bit_mask
                prog.emit(Quad::and_(e_reg, e_reg, bit_mask));
            }
        } else {
            // Multi-bit field: mask = ((1 << (hi - lo + 1)) - 1) << lo
            int num_bits = hi_val - lo_val + 1;
            auto bit_mask = prog.newReg();
            prog.emit(Quad::seti(bit_mask, (1 << num_bits) - 1));
            auto shift_reg = prog.newReg();
            prog.emit(Quad::seti(shift_reg, lo_val));
            prog.emit(Quad::shl(bit_mask, bit_mask, shift_reg));

            // Clear the bitfield area in e_reg
            auto inv_mask = prog.newReg();
            prog.emit(Quad::inv(inv_mask, bit_mask));
            prog.emit(Quad::and_(e_reg, e_reg, inv_mask));

            // Align value and set the bitfield in e_reg
            auto aligned_value = prog.newReg();
            prog.emit(Quad::seti(aligned_value, (1 << num_bits) - 1));
            prog.emit(Quad::and_(aligned_value, value_reg, aligned_value));
            prog.emit(Quad::shl(aligned_value, aligned_value, shift_reg));
            prog.emit(Quad::or_(e_reg, e_reg, aligned_value));
        }
    } else {
        // General case for dynamic hi/lo or value
        auto num_bits = prog.newReg();
        prog.emit(Quad::sub(num_bits, hi_reg, lo_reg));
        auto one_reg = prog.newReg();
        prog.emit(Quad::seti(one_reg, 1));
        prog.emit(Quad::add(num_bits, num_bits, one_reg));

        // Construct the bit mask dynamically: ((1 << num_bits) - 1) << lo
        auto bit_mask = prog.newReg();
        prog.emit(Quad::shl(bit_mask, one_reg, num_bits));
        prog.emit(Quad::sub(bit_mask, bit_mask, one_reg));
        prog.emit(Quad::shl(bit_mask, bit_mask, lo_reg));

        // Clear the target bitfield in e_reg
        auto inv_mask = prog.newReg();
        prog.emit(Quad::inv(inv_mask, bit_mask));
        prog.emit(Quad::and_(e_reg, e_reg, inv_mask));

        // Align value and set the bitfield in e_reg
        auto aligned_value = prog.newReg();
        prog.emit(Quad::and_(aligned_value, value_reg, bit_mask));
        prog.emit(Quad::shl(aligned_value, aligned_value, lo_reg));
        prog.emit(Quad::or_(e_reg, e_reg, aligned_value));
    }

    // If e_reg represents a memory-mapped register, store back the updated value
    if (_dec->type() == Declaration::REG) {
        prog.emit(Quad::store(addr_reg, e_reg));
    }
}

///
/// Génération d'un changement d'état
void GotoStatement::gen(AutoDecl& automaton, QuadProgram& prog) const {
    prog.comment(pos);
    prog.emit(Quad::goto_(_state->label()));
}

///
/// Génération d'un arrêt d'automate
void StopStatement::gen(AutoDecl& automaton, QuadProgram& prog) const {
    prog.comment(pos);
    prog.emit(Quad::goto_(automaton.stopLabel()));
}

///
/// Génération d'une clause when
void When::gen(AutoDecl& automaton, QuadProgram& prog) {
    prog.comment("Generating 'When' clause");

    // Step 1: Load the signal register's address and read its value
    auto sig_addr = prog.newReg();
    auto sig_val = prog.newReg();

    // Set signal address and load its value
    prog.emit(Quad::seti(sig_addr, _sig->reg()->address()));
    prog.emit(Quad::load(sig_val, sig_addr));

    // Step 2: Isolate the specific bit of the signal
    auto bit_pos = prog.newReg();
    auto bit_mask = prog.newReg();
    prog.emit(Quad::seti(bit_pos, _sig->bit()));       // Bit position

    auto one_reg = prog.newReg();
    prog.emit(Quad::seti(one_reg, 1));


    prog.emit(Quad::shl(bit_mask, one_reg, bit_pos)); // Create mask for the bit

    // Apply mask to extract bit value
    auto masked_bit = prog.newReg();
    prog.emit(Quad::and_(masked_bit, sig_val, bit_mask));

    // Step 3: Test the condition (apply negation if necessary)
    auto skip_label = prog.newLab();
    if (_neg) {
        prog.emit(Quad::goto_eq(skip_label, masked_bit, bit_mask)); // Skip if the bit is set
    } else {
        prog.emit(Quad::goto_ne(skip_label, masked_bit, bit_mask)); // Skip if the bit is not set
    }

    // Step 4: Execute action if condition is met
    _action->gen(automaton, prog);

    // Step 5: Insert label to skip action if condition was not met
    prog.emit(Quad::lab(skip_label));
}


///
/// Génération d'un état de l'automate
void State::gen(AutoDecl& automaton, QuadProgram& prog) {
    prog.emit(Quad::lab(_label));
    _action->gen(automaton, prog);
    auto loop = prog.newLab();
    prog.emit(Quad::lab(loop));
    for(auto when: _whens)
        when->gen(automaton, prog);
    prog.emit(Quad::goto_(loop));
}

///
/// Génération de l'automate
void AutoDecl::gen(QuadProgram& prog) {
    _stop_label = prog.newLab();
    for(auto state: _states)
        state->setLabel(prog.newLab());
    _init->gen(*this, prog);
    for(auto state: _states)
        state->gen(*this, prog);
    prog.emit(Quad::lab(_stop_label));
    prog.emit(Quad::return_());
}
