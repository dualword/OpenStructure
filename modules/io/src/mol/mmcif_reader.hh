//------------------------------------------------------------------------------
// This file is part of the OpenStructure project <www.openstructure.org>
//
// Copyright (C) 2008-2011 by the OpenStructure authors
//
// This library is free software; you can redistribute it and/or modify it under
// the terms of the GNU Lesser General Public License as published by the Free
// Software Foundation; either version 3.0 of the License, or (at your option)
// any later version.
// This library is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more
// details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this library; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
//------------------------------------------------------------------------------
#ifndef OST_MMCIF_PARSER_HH
#define OST_MMCIF_PARSER_HH

#include <map>

//#include <boost/iostreams/filtering_stream.hpp>
//#include <boost/filesystem/fstream.hpp>

#include <ost/mol/residue_handle.hh>
#include <ost/mol/chain_type.hh>
#include <ost/io/mol/io_profile.hh>
#include <ost/io/io_exception.hh>
#include <ost/io/mol/star_parser.hh>

namespace ost { namespace io {

/// \brief reader for the mmcif file format
/// 
/// \section mmcif_format mmcif format description/ coverage
/// 
/// mmcif is an instance of the \link StarParser STAR format\endlink to store
/// entries of the PDB. The following data categories should be covered by this
/// parser:
/// 
/// \li atom_site
class DLLEXPORT_OST_IO MMCifParser : public StarParser  {
public:
  /// \brief create a MMCifParser
  ///
  /// \param stream input stream
  MMCifParser(std::istream& stream, mol::EntityHandle& ent_handle,
              const IOProfile& profile);

  /// \brief create a MMCifParser
  ///
  /// \param filename input file
  MMCifParser(const String& filename, mol::EntityHandle& ent_handle,
              const IOProfile& profile);

  /// \brief Initialise the parser.
  ///
  /// \param loc Location of the file
  void Init();

  /// \brief Set up a fresh instance
  void ClearState();

  /// \brief Set names of restricted chains for the parser.
  ///
  /// \param restrict_chains chain name
  void SetRestrictChains(const String& restrict_chains);

  const String& GetRestrictChains() const
  {
    return restrict_chains_;
  }

  /// \brief check mmcif input to be read. Substitutional function for
  /// \link StarParser StarParser\endlink.
  ///
  /// \param data_name value of the data_ tag
  ///
  /// \return true, if the blockcode (PDB id) is valid, false otherwise
  virtual bool OnBeginData(const StringRef& data_name);

  /// \brief check if a current loop is to be parsed
  ///
  /// \param header categories of the upcoming loop block
  ///
  /// \return bool
  virtual bool OnBeginLoop(const StarLoopDesc& header); // tested

  /// \brief read a row of data
  ///
  /// \param header categories and items
  /// \param columns data
  virtual void OnDataRow(const StarLoopDesc& header, 
                         const std::vector<StringRef>& columns);

  /// \brief Finalise parsing.
  virtual void OnEndData();

 protected:
  /// \brief Store an item index from loop header in preparation for reading a 
  ///        row. Throws an exception if the item does not exist.
  ///
  /// \param mapping position the item index is stored at
  /// \param item exact item name to fetch
  /// \param header loop header to pull index from
  void TryStoreIdx(const int mapping,
                   const String& item,
                   const StarLoopDesc& header)
  {
    indices_[mapping] = header.GetIndex(item);
    
    if (indices_[mapping] == -1) {
      throw IOException(this->FormatDiagnostic(STAR_DIAG_ERROR,
                                               "No item '" + item +
                                               "' found in '" +
                                               header.GetCategory()+
                                               "' header",
                                               this->GetCurrentLinenum()));
     }
  } // tested

  /// \brief Check a PDB id to be of length 4 and start with a digit
  ///
  /// \param pdbid putative PDB id
  ///
  /// \return true for a valid id, false otherwise
  bool IsValidPDBIdent(const StringRef& pdbid); // tested

  /// \brief fetch values identifying atoms
  ///
  /// \param[in]  columns data row
  /// \param[out] chain_name takes atom_site.label_asym_id or, if
  ///             auth_chain_id_ is set, atom_site.auth_asym_id as a chain name
  /// \param[out] res_name fetches atom_site.label_comp_id

  /// \param[out] atom_name corresponds to label_atom_id
  bool ParseAtomIdent(const std::vector<StringRef>& columns,
                      String& chain_name,
                      StringRef& res_name,
                      mol::ResNum& resnum,
                      bool& valid_res_num,
                      StringRef& atom_name,
                      char& alt_loc);

  /// \brief Fetch atom information and store it.
  ///
  /// \param columns data row
  void ParseAndAddAtom(const std::vector<StringRef>& columns);

  /// \brief Fetch MMCif entity information
  ///
  /// \param columns data row
  void ParseEntity(const std::vector<StringRef>& columns);

private:
  /// \enum magic numbers of this class
  typedef enum {
    PDBID_LEN=4,         ///< length of a PDB id
    MAX_ITEMS_IN_ROW=18, ///< count for possible items in a loop row
  } MMCifMagicNos;

  /// \enum items of the atom_site category
  typedef enum {
    AUTH_ASYM_ID,      ///< chain name by author as in PDB
    AS_ID,             ///< atom serial id
    LABEL_ALT_ID,      ///< AltLoc
    LABEL_ASYM_ID,     ///< chain name by PDB
    LABEL_ATOM_ID,
    LABEL_COMP_ID,
    LABEL_ENTITY_ID,   ///< link to category entity
    LABEL_SEQ_ID,      ///< residue no.
    AUTH_SEQ_ID,       ///< residue no. by author
    TYPE_SYMBOL,       ///< chemical element
    CARTN_X,           ///< Coordinates ||IMPORTANT: This 3 entries have to stay
    CARTN_Y,           ///< Coordinates ||together for the parser to work!
    CARTN_Z,           ///< Coordinates ||
    OCCUPANCY,
    B_ISO_OR_EQUIV,
    PDBX_PDB_INS_CODE,
    GROUP_PDB,         ///< record name
    PDBX_PDB_MODEL_NUM ///< model no. (especially NMR structures)
  } AtomSiteItems;

  /// \enum items of the entity category
  typedef enum {
    E_ID,              ///< unique identifier
    E_TYPE,            ///< polymer, non-polymer or water
    DETAILS            ///< special aspects of the entity
  } EntityItems;

  /// \enum categories of the mmcif format
  typedef enum {
    ATOM_SITE,
    ENTITY,
    DONT_KNOW
  } MMCifCategory;

  /// \struct keeping track of entity information
  typedef struct {
    ChainType type; ///< characterise entity
    String details; ///< description of this entity
  } MMCifEntityDesc;

  typedef std::map<String, MMCifEntityDesc> MMCifEntityDescMap;

  // members
  MMCifCategory category_;
  int category_counts_[DONT_KNOW]; ///< overall no. of atom_site loops
  int indices_[MAX_ITEMS_IN_ROW]; ///< map items to values in loops
  const IOProfile& profile_;
  mol::EntityHandle& ent_handle_;
  String restrict_chains_;
  bool auth_chain_id_;       ///< use chain IDs given by authors rather than pdb
  mol::ChainHandle curr_chain_;
  mol::ResidueHandle curr_residue_;
  int chain_count_;
  int residue_count_;
  int atom_count_;
  bool warned_name_mismatch_;
  String subst_res_id_; ///< work around for missing label_seq_id's
  bool has_model_;      ///< keep track of models through different atom_sites
  int curr_model_;      ///< if we have pdbx_PDB_model_num, store no.
  std::vector<std::pair<mol::ChainHandle, String> > chain_id_pairs_;
  ///< chain and label_entity_id
  MMCifEntityDescMap entity_desc_map_; ///< stores entity items
};

}}

#endif
