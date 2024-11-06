#include "AST.hpp"
#include "Quad.hpp"

#include <assert.h>

const Quad::lab_t
    base_call = 10000,
    field_get_call = 10000,
    field_set_call = 100001;

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
            auto ra = prog.newReg(); // address
            auto rd = prog.newReg(); // data
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
    auto expr_reg = _expr->gen(prog);
    auto result_reg = prog.newReg();

    auto hi_val_opt = _hi->eval();
    auto lo_val_opt = _lo->eval();

    if (hi_val_opt && lo_val_opt) { // Both `hi` and `lo` are constants.
        int hi_val = *hi_val_opt;
        int lo_val = *lo_val_opt;

        // Ensure that `lo_val` is loaded into a register if needed.
        auto lo_val_reg = prog.newReg();
        prog.emit(Quad::seti(lo_val_reg, lo_val));

        if (hi_val == lo_val) {
            // Special case: Extract a single bit.
            auto shifted_reg = prog.newReg();
            prog.emit(Quad::shr(shifted_reg, expr_reg, lo_val_reg));

            // Mask
            auto mask_reg = prog.newReg();
            prog.emit(Quad::seti(mask_reg, 1));

            // Apply
            prog.emit(Quad::and_(result_reg, shifted_reg, mask_reg));
        } else {
            // General case: Extract multiple bits.
            int num_bits = hi_val - lo_val + 1;
            int mask_val = (1 << num_bits) - 1;

            // mask
            auto mask_reg = prog.newReg();
            prog.emit(Quad::seti(mask_reg, mask_val));

            // Shift expression
            auto shifted_reg = prog.newReg();
            prog.emit(Quad::shr(shifted_reg, expr_reg, lo_val_reg));

            // Apply the mask
            prog.emit(Quad::and_(result_reg, shifted_reg, mask_reg));
        }
    } else { // At least one of `hi` or `lo` is dynamic.
        auto hi_reg = _hi->gen(prog);
        auto lo_reg = _lo->gen(prog);

        auto diff_reg = prog.newReg();
        auto one_reg = prog.newReg();
        prog.emit(Quad::seti(one_reg, 1));
        prog.emit(Quad::sub(diff_reg, hi_reg, lo_reg));
        prog.emit(Quad::add(diff_reg, diff_reg, one_reg));  // n = hi - lo + 1.
        auto mask_reg = prog.newReg();
        prog.emit(Quad::shl(mask_reg, one_reg, diff_reg));  // 1 << n.
        prog.emit(Quad::sub(mask_reg, mask_reg, one_reg));  // (1 << n) - 1.

        // Shift the expression to the right by `lo`.
        auto shifted_reg = prog.newReg();
        prog.emit(Quad::shr(shifted_reg, expr_reg, lo_reg));

        // mask
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
    auto hi_reg = _hi->gen(prog);
    auto lo_reg = _lo->gen(prog);
    auto value_reg = _expr->gen(prog);

    Quad::reg_t e_reg;
    Quad::reg_t addr_reg;

    if (_dec->type() == Declaration::VAR) {
        e_reg = prog.regFor(static_cast<VarDecl*>(_dec)->name());
    } else if (_dec->type() == Declaration::REG) {
        addr_reg = prog.newReg();
        prog.emit(Quad::seti(addr_reg, static_cast<RegDecl*>(_dec)->address()));
        e_reg = prog.newReg();
        prog.emit(Quad::load(e_reg, addr_reg));
    } else {
        assert(false);
        return;
    }

    auto hi_val_opt = _hi->eval();
    auto lo_val_opt = _lo->eval();
    auto value_val_opt = _expr->eval();

    if (e_reg == value_reg) {
        auto temp_value_reg = prog.newReg();
        prog.emit(Quad::set(temp_value_reg, value_reg));
        value_reg = temp_value_reg;
    }

    if (hi_val_opt && lo_val_opt && value_val_opt) { // Constants case
        int hi_val = *hi_val_opt;
        int lo_val = *lo_val_opt;
        int value_val = *value_val_opt;

        // mask
        int num_bits = hi_val - lo_val + 1;
        int mask_val = ((1 << num_bits) - 1) << lo_val;
        auto mask_reg = prog.newReg();
        prog.emit(Quad::seti(mask_reg, mask_val));

        // Clear the target bits in e_reg
        auto inv_mask_reg = prog.newReg();
        prog.emit(Quad::inv(inv_mask_reg, mask_reg));
        prog.emit(Quad::and_(e_reg, e_reg, inv_mask_reg));

        // Prepare the value to set
        int value_mask = (1 << num_bits) - 1;
        int aligned_value = (value_val & value_mask) << lo_val;
        auto aligned_value_reg = prog.newReg();
        prog.emit(Quad::seti(aligned_value_reg, aligned_value));

        prog.emit(Quad::or_(e_reg, e_reg, aligned_value_reg));
    } else { // Dynamic case
        // Compute mask = ((1 << n) - 1) << lo
        auto one_reg = prog.newReg();
        prog.emit(Quad::seti(one_reg, 1));
        auto n_reg = prog.newReg();
        prog.emit(Quad::sub(n_reg, hi_reg, lo_reg));
        prog.emit(Quad::add(n_reg, n_reg, one_reg)); // n = hi - lo + 1
        auto temp_reg = prog.newReg();
        prog.emit(Quad::shl(temp_reg, one_reg, n_reg)); // temp_reg = 1 << n
        auto mask_reg = prog.newReg();
        prog.emit(Quad::sub(mask_reg, temp_reg, one_reg)); // mask_reg = (1 << n) - 1
        prog.emit(Quad::shl(mask_reg, mask_reg, lo_reg)); // mask_reg <<= lo

        auto inv_mask_reg = prog.newReg();
        prog.emit(Quad::inv(inv_mask_reg, mask_reg));
        prog.emit(Quad::and_(e_reg, e_reg, inv_mask_reg));

        auto value_mask_reg = prog.newReg();
        prog.emit(Quad::sub(value_mask_reg, temp_reg, one_reg)); // value_mask_reg = (1 << n) - 1

        auto aligned_value_reg = prog.newReg();
        prog.emit(Quad::and_(aligned_value_reg, value_reg, value_mask_reg)); // Mask value_reg
        prog.emit(Quad::shl(aligned_value_reg, aligned_value_reg, lo_reg)); // Align value

        prog.emit(Quad::or_(e_reg, e_reg, aligned_value_reg));
    }

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
    prog.comment(pos);

    auto sig_addr = prog.newReg();
    auto sig_val = prog.newReg();

    prog.emit(Quad::seti(sig_addr, _sig->reg()->address()));
    prog.emit(Quad::load(sig_val, sig_addr));

    // Mask
    auto bit_pos = prog.newReg();
    auto bit_mask = prog.newReg();
    prog.emit(Quad::seti(bit_pos, _sig->bit()));
    auto one_reg = prog.newReg();
    prog.emit(Quad::seti(one_reg, 1));
    prog.emit(Quad::shl(bit_mask, one_reg, bit_pos));

    // Apply mask
    auto masked_bit = prog.newReg();
    prog.emit(Quad::and_(masked_bit, sig_val, bit_mask));

    // Test the condition
    auto skip_label = prog.newLab();
    if (_neg) {
        prog.emit(Quad::goto_eq(skip_label, masked_bit, bit_mask)); // Skip if the bit is set
    } else {
        prog.emit(Quad::goto_ne(skip_label, masked_bit, bit_mask)); // Skip if the bit is not set
    }

    // Execute action
    _action->gen(automaton, prog);

    // Insert label to skip action if condition was not met
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
