# TP 4 : Traduction en quadruplets

---
Le TP se clôture par la remise des sources que vous aurez produits sur le dépôt correspondant sur Moodle.

Pour obtenir l'archive à déposer, tapez la commande :
	`$ make tp4`

Cela crée un fichier nommé `tp3-DATE.tgz` avec _DATE_ représentant la date du jour.

La partie à réaliser sont indiquées par une mention **[TODO]**.
---


## Présentation de l'allocateur de registre

Dans le cadre du TP, il est difficile de réaliser un allocateur global avec graphe d'interférence et un allocateur global sans interférence devrait donner de bien piètres résultats étant donné que la notion de boucle régulière, telle que vue en cours, s'applique difficilement à nos automates. Un allocateur global tel que vu en cours donne également de bien mauvaises performances mais il est possible de l'améliorer pour tenir compte de la forme du code généré : à l'exception des variables de `IOML`, les registres virtuels sont écrits une seule fois et utilisés une seule fois. Nous allons également réaliser une autre amélioration : allouer les registres seulement quand ils sont nécessaires dans le BB.

Pour ce faire, nous allons avoir besoin des éléments suivants :
* _X_ ⊂ _Var_ -- ensemble des registres virtuels associés avec une variable `IOML`,
* _m_: _Var_ ⟶ _R_ ∪ { ⟂ } -- dictionnaire associant à chaque registre virtuel un registre matérel ou ⟂ (pas d'association),
* _A_ ⊂ _R_ -- ensemble des registres matériels disponibles,
* _W_ ⊂ _X_ -- ensemble des registres virtuels qu'il faut sauver en fin de bloc de base.

Pour chaque instruction I(v~u~, v~d~) avec v~u~ ensemble des registres écrits (ou _mis à jour_), v~d~ ensemble des registres lus (ou _définis_),

1. Allouer les registres lus :

```
pour chaque vd
	si m[vd] = ⟂ alors  -- allouer un registre matériel
		si A ≠ ∅ alors prendre un registre r de A sinon lever une erreur
		m ⟵ m[v~d~ ⟼ r]
		A ⟵ A \ { r }
		insérer avant "LDR r, [SP, #kvd]" -- chargement depuis la pile
	remplacer vd par m[vd] dans I
```

2. Libérer les registres lus _morts_ qui ne sont pas des variables de `IOML` :
```
    pour chaque vd
		si vd ∉ X alors     -- libérer le registre matériel
			A ⟵ A U { m[vd] }
			m ⟵ m[vd ⟼ ⟂]
```

3. Allouer les registres écrits :
```
    pour chaque vu
		si m[vu] = ⟂ alors  -- allouer un registre matériel
			si A ≠ ∅ alors prendre un registre r de A sinon lever une erreur
			m ⟵ m[vu ⟼ r]
			A ⟵ A \ { r }
		remplacer vu par m[vu] dans I
        * si vu ∈ X alors W ⟵ W U { vu }    -- enregistrer vu pour le sauver en fin de bloc
```

A la fin du BB, il faut sauvegarder les variables `IOML` qui ont été mises à jour:
    pour chaque v~u~ ∈ W
        générer "STR, m[v~u~], [SP, #k~vu~]"



## Réalisation de l'allocateur de registre

**Important :** Il faut tout d'abord noter que l'allocateur utilise qu'un seul type, `Quad::reg\_t`, pour représenter registres virtuels et registres matériels. Simplement les 16 premiers registres, numéro 0 à 15, virtuels représentent les 16 registres matériels d'ARM. Cela permet également de ne pas avoir plusieurs classes pour représenter les instructions.

L'allocation des registres est soutenue par 2 classes. La première s'appelle `StackMapper` et prend en charge l'allocation dans la pile de la place pour stocker les variables de `IOML` : elle permet donc de savoir quel registre virtuel contient une variable `IOML` et à quel déplacement, relatif au registre `SP`, est stockée la valeur de la variable `IOML`. Il n'est pas nécessaire de modifier cette classe qui est utilisée durant l'allocation de tout le CFG.

La seconde classe s'appelle `RegAlloc` et réalise l'allocation des registres pour **un** BB : elle est recréée à chaque BB. Elle met en oeuvre la stratégie d'allocation vue dans la section précédente avec les variables membres suivantes :
* `_map` -- implante _m_ (type [map<Quad::reg_t, Quad::reg_t>](https://en.cppreference.com/w/cpp/container/map)),
* `_written` -- implante _W_ (type [list<Quad::reg_t>](https://en.cppreference.com/w/cpp/container/list)),
* `_avail` -- implante _A_  (type [list<Quad::reg_t>](https://en.cppreference.com/w/cpp/container/list)),
* `_fried_` -- non défini dans l'algorithme au dessus mais contient la liste des registres virtuels morts après l'instruction (type [list<Quad::reg_t>](https://en.cppreference.com/w/cpp/container/list)).

Les fonctions suivantes sont disponibles et seront utiles pour la suite :
* `isVar`(_r_) teste si un registre virtuel correspond à une variable `IOML`,
* `load`(_r_) génère le code pour charger le registre virtuel donné (après allocation),
* `store`(_r_) génère le code pour sauvegarder le registre virtuel donné (avant libération).

Pour l'instant, l'allocation de registre ne fait rien : elle recopie la séquence d'instruction qui lui est passée en paramètre. Il va falloir que vous développiez les fonctions manquantes.

**TODO**

1. Développez la fonction `allocate`(_r_) qui renvoie le registre matériel associé avec le registre virtuel _r_ qui sera, si nécessaire, alloué. S'il ne reste plus de registre, on pourra lever une erreur avec le code.

    ```c++
    assert("no more registers available!" && false);
    ```

2. Développez la fonction `RegAlloc::free`(_r_) qui libère le registre matériel associé avec le regisre virtuel _r_.
3. Développez la fonction `RegAlloc::processRead`(_p_) qui prend en argument un paramètre d'instruction en lecture et réalise l'allocation de registre en modifiant _p_.
4. Développez la fonction `RegAlloc::processWrite`(_p_) qui prend en argument un paramètre d'instruction en écriture et réalise l'allocation de registre en modifiant _p_.
5. Développez la fonction `RegAlloc::complete`() qui génère le code pour stocker la valeur des registres `IOML` modifiés dans le BB.
6. Développez la fonction `RegAlloc::process`(_i_) qui prend une instruction en paramètre et réalise l'allocation des registres dans cette instruction. Le code existant, à garder, rajoute l'instruction à la suite des instructions déjà traitées.

**Attention :** l'accès aux paramètres d'une instruction _i_ se fait en utilisant la notation tableau _i_[indice]. Cela renvoie une référence (non constante) sur le paramètre qui peut être passé tel quel à `RegAlloc::processRead`() ou à `RegAlloc::processWrite`() et être ainsi facilement mis à jour avec l'allocation.

**Pour tester :** 2 options ont été rajoutées à `IOC`
* `-print-alloc` -- affiche le CFG après allocation des regstres,
* `-S` ou `--assembly` -- affiche le code assembleur final (donc compilable si tout va bien).

7. La gestion du manque de registre matériel (question 1) est très limitée : proposez une stratégie pour améliorer cela. On pourra être amené à vider des registres dans la pile en utilisant la fonction `RegAlloc::spill`().

