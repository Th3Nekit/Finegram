//! Wallet rev00 signing-key rotation data generation.
//!
//! The host owns protected storage and transport. This module owns the pure
//! mnemonic, cell, and signature construction required by the contract.

use ed25519_dalek::{Signer as _, SigningKey};
use ton::block_tlb::{CommonMsgInfo, CommonMsgInfoExtIn, CommonMsgInfoInt, Msg, StateInit};
use ton::ton_core::cell::TonCell;
use ton::ton_core::errors::TonCoreError;
use ton::ton_core::traits::tlb::TLB as _;
use ton::ton_core::types::TonAddress;
use ton::ton_core::types::tlb_core::{MsgAddressExt, TLBCoins, TLBEitherRef};
use zeroize::Zeroizing;

use super::KeyRotationMessageKind;
use super::crypto::{
    SensitiveMnemonic, derive_half_key, derive_rotation_keys, derive_wallet,
    derive_wallet_public_state,
};
use super::key_history::encrypt_old_private_key;
use super::mnemonic::{Bip39Half, ENTROPY_LEN, RotationMnemonic};
use crate::{Boc, Network, TonAddressString};

const KEY_ROTATION_PROOF_TAG: &[u8; 12] = b"KEY_ROTATION";
const KEY_ROTATION_PROOF_TAG_BITS: usize = 96;
const CHANGE_PUBLIC_KEY_INTERNAL_OPCODE: u32 = 0xfbba_99c7;
const CHANGE_PUBLIC_KEY_EXTERNAL_OPCODE: u32 = 0xfbba_99c8;
const SIGNATURE_BITS: usize = 512;
const PUBLIC_KEY_BITS: usize = 256;
const ENCRYPTED_PRIVATE_KEY_BITS: usize = 256;
const MAX_KEY_GENERATION_ATTEMPTS: usize = 8;

#[derive(Debug, thiserror::Error)]
pub(crate) enum KeyRotationError {
    #[error("the protected mnemonic is invalid")]
    InvalidMnemonic,
    #[error("the protected mnemonic does not belong to this wallet")]
    WalletIdentityMismatch,
    #[error("the expiration timestamp exceeds uint32")]
    ExpirationOutOfRange,
    #[error("a rotated recovery phrase requires an already deployed wallet")]
    RotatedWalletRequiresActiveAccount,
    #[error("key-rotation data construction failed")]
    Preparation,
}

pub(crate) struct PreparedKeyRotationMaterial {
    pub(crate) replacement_mnemonic: SensitiveMnemonic,
    pub(crate) new_public_key: [u8; 32],
    pub(crate) signed_boc: Boc,
    pub(crate) external_boc: Boc,
    pub(crate) internal_boc: Boc,
}

pub(crate) fn prepare_key_rotation(
    current_mnemonic: &SensitiveMnemonic,
    network: Network,
    expected_wallet: &TonAddressString,
    seqno: u32,
    needs_state_init: bool,
    valid_until: u64,
    message_kind: KeyRotationMessageKind,
) -> Result<PreparedKeyRotationMaterial, KeyRotationError> {
    let current_phrase = current_mnemonic
        .as_str()
        .map_err(|_| KeyRotationError::InvalidMnemonic)?;
    let current =
        RotationMnemonic::parse(current_phrase).map_err(|_| KeyRotationError::InvalidMnemonic)?;
    let wallet =
        derive_wallet(current_phrase, network).map_err(|_| KeyRotationError::InvalidMnemonic)?;
    if wallet.address != *expected_wallet.as_address() {
        return Err(KeyRotationError::WalletIdentityMismatch);
    }
    // Deployment stores the anchor key, so only the 12-word phrase, whose
    // signing key is the anchor, can sign a request that also deploys.
    if needs_state_init && !wallet.is_pre_rotation() {
        return Err(KeyRotationError::RotatedWalletRequiresActiveAccount);
    }
    let valid_until =
        u32::try_from(valid_until).map_err(|_| KeyRotationError::ExpirationOutOfRange)?;
    let current_keys = derive_rotation_keys(&current);

    for _ in 0..MAX_KEY_GENERATION_ATTEMPTS {
        let mut entropy = Zeroizing::new([0_u8; ENTROPY_LEN]);
        getrandom::fill(entropy.as_mut()).map_err(|_| KeyRotationError::Preparation)?;
        let new_half =
            Bip39Half::from_entropy(&entropy).map_err(|_| KeyRotationError::Preparation)?;
        let new_key = derive_half_key(&new_half);
        if new_key.verifying_key().to_bytes() == current_keys.signing.verifying_key().to_bytes() {
            continue;
        }

        let state_init = if needs_state_init {
            let (derived_address, state_init) = derive_wallet_public_state(
                &current_keys.anchor.verifying_key().to_bytes(),
                network,
            )
            .map_err(|_| KeyRotationError::Preparation)?;
            if derived_address != wallet.address {
                return Err(KeyRotationError::WalletIdentityMismatch);
            }
            Some(state_init)
        } else {
            None
        };

        return prepare_with_new_half(
            &current,
            &current_keys.signing,
            &new_half,
            &new_key,
            &wallet.address,
            wallet.wallet_id,
            seqno,
            state_init,
            valid_until,
            message_kind,
        );
    }

    Err(KeyRotationError::Preparation)
}

#[allow(
    clippy::too_many_arguments,
    reason = "all signed header and identity fields stay explicit at the cryptographic boundary"
)]
fn prepare_with_new_half(
    current: &RotationMnemonic,
    current_key: &SigningKey,
    new_half: &Bip39Half,
    new_key: &SigningKey,
    wallet_address: &TonAddress,
    wallet_id: i32,
    seqno: u32,
    state_init: Option<StateInit>,
    valid_until: u32,
    message_kind: KeyRotationMessageKind,
) -> Result<PreparedKeyRotationMaterial, KeyRotationError> {
    let new_public_key = new_key.verifying_key().to_bytes();
    let proof = build_rotation_proof(wallet_address).map_err(|_| KeyRotationError::Preparation)?;
    let proof_signature = new_key.sign(
        proof
            .cell_hash()
            .map_err(|_| KeyRotationError::Preparation)?
            .as_slice(),
    );
    // Generate the replacement only once. The opcodes differ, so each delivery
    // form needs its own current-key signature over the same replacement key.
    let build = |kind, init| {
        let request = build_change_public_key_request(
            kind,
            wallet_id,
            valid_until,
            seqno,
            new_public_key,
            proof_signature.to_bytes(),
            encrypt_old_private_key(current_key, new_key),
        )
        .map_err(|_| KeyRotationError::Preparation)?;
        let signed_request = sign_cell(current_key, &request)?;
        let message = wrap_signed_request(wallet_address, kind, signed_request, init)?;
        Boc::try_from(
            message
                .to_boc()
                .map_err(|_| KeyRotationError::Preparation)?,
        )
        .map_err(|_| KeyRotationError::Preparation)
    };
    let external_boc = build(KeyRotationMessageKind::External, state_init.clone())?;
    let internal_boc = build(KeyRotationMessageKind::Internal, state_init)?;
    let signed_boc = match message_kind {
        KeyRotationMessageKind::External => external_boc.clone(),
        KeyRotationMessageKind::Internal => internal_boc.clone(),
    };

    let mut replacement_phrase = Zeroizing::new(String::new());
    replacement_phrase.push_str(&current.anchor().to_phrase());
    replacement_phrase.push(' ');
    replacement_phrase.push_str(&new_half.to_phrase());
    let replacement_mnemonic =
        SensitiveMnemonic::from_bytes(replacement_phrase.as_bytes().to_vec())
            .map_err(|_| KeyRotationError::Preparation)?;

    Ok(PreparedKeyRotationMaterial {
        replacement_mnemonic,
        new_public_key,
        signed_boc,
        external_boc,
        internal_boc,
    })
}

/// Constructs an external rotation preview using public state only. Both the
/// owner signature and the new-key proof are zero placeholders; this BOC must
/// never authorize a real rotation, even if the emulation provider broadcasts it.
pub(crate) fn prepare_key_rotation_emulation(
    source: &TonAddressString,
    anchor_public_key: &[u8],
    network: Network,
    seqno: u32,
    needs_state_init: bool,
    valid_until: u64,
) -> Result<Boc, KeyRotationError> {
    let valid_until =
        u32::try_from(valid_until).map_err(|_| KeyRotationError::ExpirationOutOfRange)?;
    let wallet_id = match network {
        Network::Mainnet => ton::ton_wallet::WALLET_SUBWALLET_ID_DEFAULT,
        Network::Testnet => ton::ton_wallet::WALLET_SUBWALLET_ID_DEFAULT_TESTNET,
    };
    // Encoded Ed25519 base point: a valid public key unrelated to the wallet.
    let mut placeholder_key = [0x66; 32];
    placeholder_key[0] = 0x58;
    let request = build_change_public_key_request(
        KeyRotationMessageKind::External,
        wallet_id,
        valid_until,
        seqno,
        placeholder_key,
        [0; 64],
        [0; 32],
    )
    .map_err(|_| KeyRotationError::Preparation)?;
    let mut body = TonCell::builder();
    body.write_bits([0; 64], SIGNATURE_BITS)
        .map_err(|_| KeyRotationError::Preparation)?;
    body.write_cell(&request)
        .map_err(|_| KeyRotationError::Preparation)?;
    let body = body.build().map_err(|_| KeyRotationError::Preparation)?;
    let init = if needs_state_init {
        let (address, init) = derive_wallet_public_state(anchor_public_key, network)
            .map_err(|_| KeyRotationError::Preparation)?;
        if &address != source.as_address() {
            return Err(KeyRotationError::WalletIdentityMismatch);
        }
        Some(init)
    } else {
        None
    };
    let message = wrap_signed_request(
        source.as_address(),
        KeyRotationMessageKind::External,
        body,
        init,
    )?;
    Boc::try_from(
        message
            .to_boc()
            .map_err(|_| KeyRotationError::Preparation)?,
    )
    .map_err(|_| KeyRotationError::Preparation)
}

fn build_rotation_proof(wallet_address: &TonAddress) -> Result<TonCell, TonCoreError> {
    let workchain = i8::try_from(wallet_address.workchain).map_err(|_| {
        TonCoreError::Custom("Wallet key-rotation proof requires an int8 workchain".to_owned())
    })?;
    let mut builder = TonCell::builder();
    builder.write_bits(KEY_ROTATION_PROOF_TAG, KEY_ROTATION_PROOF_TAG_BITS)?;
    builder.write_num(&u8::from_be_bytes(workchain.to_be_bytes()), 8)?;
    builder.write_bits(wallet_address.hash.as_slice(), PUBLIC_KEY_BITS)?;
    builder.build()
}

/// Builds `ChangePublicKeyRequestE` or `ChangePublicKeyRequestI`.
///
/// The rotation proof and the encrypted old private key are separate refs, in
/// that order. The contract loads the second ref as exactly 256 bits and
/// publishes it in its key-changed log.
fn build_change_public_key_request(
    message_kind: KeyRotationMessageKind,
    wallet_id: i32,
    valid_until: u32,
    seqno: u32,
    new_public_key: [u8; 32],
    proof_signature: [u8; 64],
    encrypted_old_private_key: [u8; 32],
) -> Result<TonCell, TonCoreError> {
    let opcode = match message_kind {
        KeyRotationMessageKind::External => CHANGE_PUBLIC_KEY_EXTERNAL_OPCODE,
        KeyRotationMessageKind::Internal => CHANGE_PUBLIC_KEY_INTERNAL_OPCODE,
    };
    let mut signature = TonCell::builder();
    signature.write_bits(proof_signature, SIGNATURE_BITS)?;
    let mut encrypted_old_key = TonCell::builder();
    encrypted_old_key.write_bits(encrypted_old_private_key, ENCRYPTED_PRIVATE_KEY_BITS)?;

    let mut request = TonCell::builder();
    request.write_num(&opcode, 32)?;
    request.write_num(&u32::from_be_bytes(wallet_id.to_be_bytes()), 32)?;
    request.write_num(&valid_until, 32)?;
    request.write_num(&seqno, 32)?;
    request.write_bits(new_public_key, PUBLIC_KEY_BITS)?;
    request.write_ref(signature.build()?)?;
    request.write_ref(encrypted_old_key.build()?)?;
    request.build()
}

fn sign_cell(key: &SigningKey, body: &TonCell) -> Result<TonCell, KeyRotationError> {
    let hash = body
        .cell_hash()
        .map_err(|_| KeyRotationError::Preparation)?;
    let mut signed = TonCell::builder();
    signed
        .write_bits(key.sign(hash.as_slice()).to_bytes(), SIGNATURE_BITS)
        .map_err(|_| KeyRotationError::Preparation)?;
    signed
        .write_cell(body)
        .map_err(|_| KeyRotationError::Preparation)?;
    signed.build().map_err(|_| KeyRotationError::Preparation)
}

fn wrap_signed_request(
    wallet_address: &TonAddress,
    message_kind: KeyRotationMessageKind,
    signed_request: TonCell,
    state_init: Option<StateInit>,
) -> Result<TonCell, KeyRotationError> {
    let info = match message_kind {
        KeyRotationMessageKind::External => CommonMsgInfo::ExtIn(CommonMsgInfoExtIn {
            src: MsgAddressExt::NONE,
            dst: wallet_address.to_msg_address_int(),
            import_fee: TLBCoins::ZERO,
        }),
        KeyRotationMessageKind::Internal => {
            let mut info = CommonMsgInfoInt::new(wallet_address.to_msg_address(), TLBCoins::ZERO);
            info.bounce = false;
            CommonMsgInfo::Int(info)
        }
    };

    let mut message = Msg::new(info, signed_request);
    if let Some(state_init) = state_init {
        message.init = Some(TLBEitherRef::new(state_init));
    }
    message.to_cell().map_err(|_| KeyRotationError::Preparation)
}

#[cfg(test)]
mod tests {
    use ed25519_dalek::{Signature, VerifyingKey};
    use ton::ton_core::cell::CellParser;
    use ton::ton_core::traits::tlb::TLB as _;
    use ton::ton_core::types::tlb_core::MsgAddress;

    use super::super::key_history::decrypt_old_private_key;
    use super::*;

    const CURRENT_PHRASE: &str =
        "notice tortoise soup strong gun divide offer process salon siren general carry";
    const ROTATED_PHRASE: &str = "notice tortoise soup strong gun divide offer process salon siren general carry clump left year void clutch tool case burden fix income champion lounge";
    const SEQNO: u32 = 0x0102_0304;
    const VALID_UNTIL: u32 = 0x7100_0000;

    #[test]
    fn external_rotation_matches_the_contract_shape_and_binds_both_keys() {
        let (material, request, proof, current_public_key, new_public_key) =
            deterministic_material(KeyRotationMessageKind::External);
        let message = Msg::<TonCell>::from_boc(material.signed_boc.as_bytes().to_vec())
            .expect("rotation BOC decodes");
        let CommonMsgInfo::ExtIn(info) = &message.info else {
            panic!("external rotation must use an external envelope");
        };
        let wallet = derive_wallet(CURRENT_PHRASE, Network::Testnet).expect("wallet derives");
        assert_eq!(info.dst, wallet.address.to_msg_address_int());
        assert!(message.init.is_none());

        let encrypted_old_private_key = assert_signed_request(
            &message.body.value,
            &request,
            current_public_key,
            CHANGE_PUBLIC_KEY_EXTERNAL_OPCODE,
            new_public_key,
        );
        assert_old_key_is_recoverable(encrypted_old_private_key);
        assert_rotation_proof(&proof, &request, new_public_key);
        assert_eq!(material.new_public_key, new_public_key);
        assert_eq!(
            proof.cell_hash().expect("proof hashes").to_string(),
            "26ADD39A33B9F4A6BE22ADC98DB908C05AE47976EFD215CBB82D1A2691717517"
        );
        assert_eq!(
            request.cell_hash().expect("request hashes").to_string(),
            "A6E5EA5F66D9DFD7095C3480F805909903D7A6FE7940A4A909921ED328535C05"
        );

        let replacement = material
            .replacement_mnemonic
            .as_str()
            .expect("replacement phrase is UTF-8");
        assert_eq!(replacement.split_whitespace().count(), 24);
        assert!(replacement.starts_with(CURRENT_PHRASE));
        assert!(
            !RotationMnemonic::parse(replacement)
                .expect("replacement phrase parses")
                .is_pre_rotation()
        );

        assert_eq!(
            message.cell_hash().expect("message hashes").to_string(),
            "2BC55A0B534FD1FC0E0EC370C2E86AB0107F1912C3AE7CE2DB0D2491D32CB401"
        );
    }

    #[test]
    fn internal_rotation_uses_the_channel_specific_opcode() {
        let (material, request, _, current_public_key, new_public_key) =
            deterministic_material(KeyRotationMessageKind::Internal);
        let message = Msg::<TonCell>::from_boc(material.signed_boc.as_bytes().to_vec())
            .expect("rotation BOC decodes");
        let CommonMsgInfo::Int(info) = &message.info else {
            panic!("internal rotation must use an internal envelope");
        };
        let wallet = derive_wallet(CURRENT_PHRASE, Network::Testnet).expect("wallet derives");
        assert_eq!(info.src, MsgAddress::NONE);
        assert_eq!(info.dst, wallet.address.to_msg_address());
        assert_eq!(info.value.coins, TLBCoins::ZERO);
        assert!(!info.bounce);
        assert!(message.init.is_none());

        let encrypted_old_private_key = assert_signed_request(
            &message.body.value,
            &request,
            current_public_key,
            CHANGE_PUBLIC_KEY_INTERNAL_OPCODE,
            new_public_key,
        );
        assert_old_key_is_recoverable(encrypted_old_private_key);
        assert_eq!(
            message.cell_hash().expect("message hashes").to_string(),
            "68F5DD38A320F74A3E313D02D7CB60FFC75AA124DBB77F1D2CD668F47BE4F915"
        );
    }

    #[test]
    fn production_generation_adds_an_independent_signing_half() {
        let current = SensitiveMnemonic::from_bytes(CURRENT_PHRASE.as_bytes().to_vec())
            .expect("current phrase parses");
        let wallet = derive_wallet(CURRENT_PHRASE, Network::Testnet).expect("wallet derives");
        let address = TonAddressString::from_address(&wallet.address, Network::Testnet);
        let material = prepare_key_rotation(
            &current,
            Network::Testnet,
            &address,
            0,
            false,
            u64::from(VALID_UNTIL),
            KeyRotationMessageKind::External,
        )
        .expect("rotation material generates");
        let replacement = material
            .replacement_mnemonic
            .as_str()
            .expect("replacement phrase is UTF-8");
        let parsed = RotationMnemonic::parse(replacement).expect("replacement phrase parses");
        let replacement_keys = derive_rotation_keys(&parsed);

        assert_eq!(replacement.split_whitespace().count(), 24);
        assert!(!parsed.is_pre_rotation());
        assert_eq!(
            replacement_keys.anchor.verifying_key().to_bytes(),
            wallet.key_pair.public_key,
            "rotation must preserve the address anchor"
        );
        assert_eq!(
            replacement_keys.signing.verifying_key().to_bytes(),
            material.new_public_key,
            "words 13-24 must recover the key sent to the contract"
        );
        assert_ne!(
            material.new_public_key, wallet.key_pair.public_key,
            "the contract rejects rotation to its current key"
        );
    }

    #[test]
    fn repeated_rotation_uses_current_signing_key_and_preserves_anchor() {
        let rotated = SensitiveMnemonic::from_bytes(ROTATED_PHRASE.as_bytes().to_vec())
            .expect("rotated phrase parses");
        let wallet = derive_wallet(ROTATED_PHRASE, Network::Testnet).expect("wallet derives");
        let address = TonAddressString::from_address(&wallet.address, Network::Testnet);
        let current = RotationMnemonic::parse(ROTATED_PHRASE).expect("rotated phrase parses");
        let current_keys = derive_rotation_keys(&current);
        assert_ne!(
            current_keys.anchor.verifying_key().to_bytes(),
            current_keys.signing.verifying_key().to_bytes(),
            "the fixture must already contain a rotated signing key"
        );

        let material = prepare_key_rotation(
            &rotated,
            Network::Testnet,
            &address,
            SEQNO,
            false,
            u64::from(VALID_UNTIL),
            KeyRotationMessageKind::External,
        )
        .expect("an already rotated phrase can rotate again");
        let message = Msg::<TonCell>::from_boc(material.signed_boc.as_bytes().to_vec())
            .expect("rotation BOC decodes");

        let mut parser = message.body.value.parser();
        let outer_signature = parser.read_bits(SIGNATURE_BITS).expect("outer signature");
        assert_eq!(
            parser.read_num::<u32>(32).expect("opcode"),
            CHANGE_PUBLIC_KEY_EXTERNAL_OPCODE
        );
        assert_eq!(
            parser.read_num::<u32>(32).expect("wallet id"),
            u32::from_be_bytes(wallet.wallet_id.to_be_bytes())
        );
        assert_eq!(
            parser.read_num::<u32>(32).expect("valid until"),
            VALID_UNTIL
        );
        assert_eq!(parser.read_num::<u32>(32).expect("seqno"), SEQNO);
        let new_public_key = parser
            .read_bits(PUBLIC_KEY_BITS)
            .expect("new public key")
            .try_into()
            .expect("new public key has 32 bytes");
        let proof_signature_cell = parser.read_next_ref().expect("proof signature ref");
        let mut proof_signature_parser = proof_signature_cell.parser();
        let proof_signature = proof_signature_parser
            .read_bits(SIGNATURE_BITS)
            .expect("proof signature")
            .try_into()
            .expect("proof signature has 64 bytes");
        proof_signature_parser
            .ensure_empty()
            .expect("proof signature ends exactly");
        let encrypted_old_private_key = read_encrypted_old_private_key(&mut parser);
        parser.ensure_empty().expect("signed request ends exactly");

        assert_eq!(new_public_key, material.new_public_key);
        let request = build_change_public_key_request(
            KeyRotationMessageKind::External,
            wallet.wallet_id,
            VALID_UNTIL,
            SEQNO,
            new_public_key,
            proof_signature,
            encrypted_old_private_key,
        )
        .expect("request rebuilds");
        let signature =
            Signature::from_slice(&outer_signature).expect("outer signature has 64 bytes");
        let request_hash = request.cell_hash().expect("request hashes");
        current_keys
            .signing
            .verifying_key()
            .verify_strict(request_hash.as_slice(), &signature)
            .expect("the current signing key authorizes repeated rotation");
        assert!(
            current_keys
                .anchor
                .verifying_key()
                .verify_strict(request_hash.as_slice(), &signature)
                .is_err(),
            "the anchor key must not authorize a repeated rotation"
        );

        let replacement = material
            .replacement_mnemonic
            .as_str()
            .expect("replacement phrase is UTF-8");
        let replacement = RotationMnemonic::parse(replacement).expect("replacement phrase parses");
        let replacement_keys = derive_rotation_keys(&replacement);
        assert_eq!(
            replacement_keys.anchor.verifying_key().to_bytes(),
            current_keys.anchor.verifying_key().to_bytes(),
            "repeated rotation must preserve the address anchor"
        );
        assert_eq!(
            replacement_keys.signing.verifying_key().to_bytes(),
            material.new_public_key,
            "the replacement phrase must recover the new signing key"
        );
        assert_ne!(
            material.new_public_key,
            current_keys.signing.verifying_key().to_bytes(),
            "rotation must replace the current signing key"
        );
        assert_eq!(
            decrypt_old_private_key(&encrypted_old_private_key, &replacement_keys.signing)
                .as_bytes(),
            current_keys.signing.as_bytes(),
            "the new key must open the replaced signing key, not the anchor"
        );
    }

    #[test]
    fn only_the_initial_phrase_can_rotate_and_deploy_together() {
        let initial = SensitiveMnemonic::from_bytes(CURRENT_PHRASE.as_bytes().to_vec())
            .expect("initial phrase parses");
        let rotated = SensitiveMnemonic::from_bytes(ROTATED_PHRASE.as_bytes().to_vec())
            .expect("rotated phrase parses");
        let wallet = derive_wallet(CURRENT_PHRASE, Network::Testnet).expect("wallet derives");
        let address = TonAddressString::from_address(&wallet.address, Network::Testnet);

        for message_kind in [
            KeyRotationMessageKind::External,
            KeyRotationMessageKind::Internal,
        ] {
            assert!(matches!(
                prepare_key_rotation(
                    &rotated,
                    Network::Testnet,
                    &address,
                    0,
                    true,
                    u64::from(VALID_UNTIL),
                    message_kind,
                ),
                Err(KeyRotationError::RotatedWalletRequiresActiveAccount)
            ));
        }

        let deploying = prepare_key_rotation(
            &initial,
            Network::Testnet,
            &address,
            0,
            true,
            u64::from(VALID_UNTIL),
            KeyRotationMessageKind::External,
        )
        .expect("the initial phrase deploys and rotates");
        let message = Msg::<TonCell>::from_boc(deploying.signed_boc.as_bytes().to_vec())
            .expect("rotation BOC decodes");
        assert!(message.init.is_some(), "the request deploys the wallet");
    }

    #[test]
    fn preparation_rejects_wrong_wallet_and_oversized_expiration() {
        let current = SensitiveMnemonic::from_bytes(CURRENT_PHRASE.as_bytes().to_vec())
            .expect("current phrase parses");
        let wallet = derive_wallet(CURRENT_PHRASE, Network::Testnet).expect("wallet derives");
        let address = TonAddressString::from_address(&wallet.address, Network::Testnet);
        let wrong = TonAddressString::try_from(
            "0:1111111111111111111111111111111111111111111111111111111111111111",
        )
        .expect("wrong address parses");
        assert!(matches!(
            prepare_key_rotation(
                &current,
                Network::Testnet,
                &wrong,
                0,
                false,
                u64::from(VALID_UNTIL),
                KeyRotationMessageKind::External,
            ),
            Err(KeyRotationError::WalletIdentityMismatch)
        ));
        assert!(matches!(
            prepare_key_rotation(
                &current,
                Network::Testnet,
                &address,
                0,
                false,
                u64::from(u32::MAX) + 1,
                KeyRotationMessageKind::External,
            ),
            Err(KeyRotationError::ExpirationOutOfRange)
        ));
    }

    #[test]
    fn paired_rotation_uses_one_replacement_and_verifiable_channel_signatures() {
        for phrase in [CURRENT_PHRASE, ROTATED_PHRASE] {
            let current = SensitiveMnemonic::from_bytes(phrase.as_bytes().to_vec()).unwrap();
            let wallet = derive_wallet(phrase, Network::Testnet).unwrap();
            let source = TonAddressString::from_address(&wallet.address, Network::Testnet);
            let material = prepare_key_rotation(
                &current,
                Network::Testnet,
                &source,
                SEQNO,
                false,
                u64::from(VALID_UNTIL),
                KeyRotationMessageKind::External,
            )
            .unwrap();
            let replacement =
                RotationMnemonic::parse(material.replacement_mnemonic.as_str().unwrap()).unwrap();
            let keys = derive_rotation_keys(&replacement);
            assert_eq!(
                keys.signing.verifying_key().to_bytes(),
                material.new_public_key
            );
            let current_keys = derive_rotation_keys(&RotationMnemonic::parse(phrase).unwrap());
            assert_eq!(
                keys.anchor.verifying_key(),
                current_keys.anchor.verifying_key()
            );
            assert_ne!(
                keys.signing.verifying_key(),
                current_keys.signing.verifying_key()
            );
            assert_eq!(material.signed_boc, material.external_boc);
            let proof = build_rotation_proof(&wallet.address).unwrap();
            let proof_signature = keys
                .signing
                .sign(proof.cell_hash().unwrap().as_slice())
                .to_bytes();
            for (kind, boc, opcode) in [
                (
                    KeyRotationMessageKind::External,
                    &material.external_boc,
                    CHANGE_PUBLIC_KEY_EXTERNAL_OPCODE,
                ),
                (
                    KeyRotationMessageKind::Internal,
                    &material.internal_boc,
                    CHANGE_PUBLIC_KEY_INTERNAL_OPCODE,
                ),
            ] {
                let message = Msg::<TonCell>::from_boc(boc.as_bytes().to_vec()).unwrap();
                assert!(matches!(
                    (&message.info, kind),
                    (CommonMsgInfo::ExtIn(_), KeyRotationMessageKind::External)
                        | (CommonMsgInfo::Int(_), KeyRotationMessageKind::Internal)
                ));
                let request = build_change_public_key_request(
                    kind,
                    wallet.wallet_id,
                    VALID_UNTIL,
                    SEQNO,
                    material.new_public_key,
                    proof_signature,
                    encrypt_old_private_key(&current_keys.signing, &keys.signing),
                )
                .unwrap();
                assert_signed_request(
                    &message.body.value,
                    &request,
                    current_keys.signing.verifying_key().to_bytes(),
                    opcode,
                    material.new_public_key,
                );
            }
        }
    }

    #[test]
    fn rotation_preview_has_no_valid_owner_or_replacement_signature() {
        let wallet = derive_wallet(CURRENT_PHRASE, Network::Testnet).unwrap();
        let keys = derive_rotation_keys(&RotationMnemonic::parse(CURRENT_PHRASE).unwrap());
        let source = TonAddressString::from_address(&wallet.address, Network::Testnet);
        for deploy in [false, true] {
            let boc = prepare_key_rotation_emulation(
                &source,
                &keys.anchor.verifying_key().to_bytes(),
                Network::Testnet,
                SEQNO,
                deploy,
                u64::from(VALID_UNTIL),
            )
            .unwrap();
            let message = Msg::<TonCell>::from_boc(boc.as_bytes().to_vec()).unwrap();
            assert!(matches!(message.info, CommonMsgInfo::ExtIn(_)));
            assert_eq!(message.init.is_some(), deploy);
            let mut parser = message.body.value.parser();
            let owner_signature = parser.read_bits(SIGNATURE_BITS).unwrap();
            assert_eq!(owner_signature, [0; 64]);
            assert_eq!(
                parser.read_num::<u32>(32).unwrap(),
                CHANGE_PUBLIC_KEY_EXTERNAL_OPCODE
            );
            let _ = parser.read_num::<u32>(32).unwrap();
            assert_eq!(parser.read_num::<u32>(32).unwrap(), VALID_UNTIL);
            assert_eq!(parser.read_num::<u32>(32).unwrap(), SEQNO);
            let key: [u8; 32] = parser.read_bits(256).unwrap().try_into().unwrap();
            let proof_cell = parser.read_next_ref().unwrap();
            let proof_signature = proof_cell.parser().read_bits(SIGNATURE_BITS).unwrap();
            assert_eq!(proof_signature, [0; 64]);
            assert_eq!(read_encrypted_old_private_key(&mut parser), [0; 32]);
            parser.ensure_empty().unwrap();
            let proof = build_rotation_proof(&wallet.address).unwrap();
            assert!(
                VerifyingKey::from_bytes(&key)
                    .unwrap()
                    .verify_strict(
                        proof.cell_hash().unwrap().as_slice(),
                        &Signature::from_slice(&proof_signature).unwrap()
                    )
                    .is_err()
            );
            let request = build_change_public_key_request(
                KeyRotationMessageKind::External,
                wallet.wallet_id,
                VALID_UNTIL,
                SEQNO,
                key,
                [0; 64],
                [0; 32],
            )
            .unwrap();
            assert!(
                keys.signing
                    .verifying_key()
                    .verify_strict(
                        request.cell_hash().unwrap().as_slice(),
                        &Signature::from_slice(&owner_signature).unwrap()
                    )
                    .is_err()
            );
        }
    }

    fn deterministic_material(
        message_kind: KeyRotationMessageKind,
    ) -> (
        PreparedKeyRotationMaterial,
        TonCell,
        TonCell,
        [u8; 32],
        [u8; 32],
    ) {
        let current = RotationMnemonic::parse(CURRENT_PHRASE).expect("current phrase parses");
        let current_key = derive_rotation_keys(&current).signing;
        let current_public_key = current_key.verifying_key().to_bytes();
        let new_half =
            Bip39Half::from_entropy(&[0x7f; ENTROPY_LEN]).expect("fixed entropy encodes");
        let new_key = derive_half_key(&new_half);
        let new_public_key = new_key.verifying_key().to_bytes();
        let wallet = derive_wallet(CURRENT_PHRASE, Network::Testnet).expect("wallet derives");
        let proof = build_rotation_proof(&wallet.address).expect("proof payload builds");
        let proof_signature = new_key
            .sign(proof.cell_hash().expect("proof payload hashes").as_slice())
            .to_bytes();
        let request = build_change_public_key_request(
            message_kind,
            wallet.wallet_id,
            VALID_UNTIL,
            SEQNO,
            new_public_key,
            proof_signature,
            encrypt_old_private_key(&current_key, &new_key),
        )
        .expect("request builds");
        let material = prepare_with_new_half(
            &current,
            &current_key,
            &new_half,
            &new_key,
            &wallet.address,
            wallet.wallet_id,
            SEQNO,
            None,
            VALID_UNTIL,
            message_kind,
        )
        .expect("rotation material builds");

        (material, request, proof, current_public_key, new_public_key)
    }

    fn assert_signed_request(
        signed: &TonCell,
        expected_request: &TonCell,
        current_public_key: [u8; 32],
        expected_opcode: u32,
        new_public_key: [u8; 32],
    ) -> [u8; 32] {
        let mut parser = signed.parser();
        let signature = parser.read_bits(SIGNATURE_BITS).expect("outer signature");
        assert_eq!(parser.read_num::<u32>(32).expect("opcode"), expected_opcode);
        let _wallet_id = parser.read_num::<u32>(32).expect("wallet id");
        assert_eq!(
            parser.read_num::<u32>(32).expect("valid until"),
            VALID_UNTIL
        );
        assert_eq!(parser.read_num::<u32>(32).expect("seqno"), SEQNO);
        assert_eq!(
            parser.read_bits(PUBLIC_KEY_BITS).expect("new public key"),
            new_public_key
        );
        let proof_signature = parser.read_next_ref().expect("proof signature ref");
        assert_eq!(proof_signature.data_len_bits(), SIGNATURE_BITS);
        assert!(proof_signature.refs().is_empty());
        let encrypted_old_private_key = read_encrypted_old_private_key(&mut parser);
        parser.ensure_empty().expect("signed request ends exactly");

        let signature = Signature::from_slice(&signature).expect("signature has 64 bytes");
        let current = VerifyingKey::from_bytes(&current_public_key).expect("current key parses");
        current
            .verify_strict(
                expected_request
                    .cell_hash()
                    .expect("request hashes")
                    .as_slice(),
                &signature,
            )
            .expect("current key verifies the request");
        encrypted_old_private_key
    }

    /// Reads the second request ref, which the contract loads as exactly 256 bits.
    fn read_encrypted_old_private_key(parser: &mut CellParser<'_>) -> [u8; 32] {
        let cell = parser
            .read_next_ref()
            .expect("encrypted old private key ref");
        assert_eq!(cell.data_len_bits(), ENCRYPTED_PRIVATE_KEY_BITS);
        assert!(cell.refs().is_empty());
        let mut cell_parser = cell.parser();
        let encrypted = cell_parser
            .read_bits(ENCRYPTED_PRIVATE_KEY_BITS)
            .expect("encrypted old private key")
            .try_into()
            .expect("encrypted old private key has 32 bytes");
        cell_parser
            .ensure_empty()
            .expect("encrypted old private key ends exactly");
        encrypted
    }

    /// The deterministic rotation replaces the 12-word key with the fixed-entropy half.
    fn assert_old_key_is_recoverable(encrypted_old_private_key: [u8; 32]) {
        let current = RotationMnemonic::parse(CURRENT_PHRASE).expect("current phrase parses");
        let current_key = derive_rotation_keys(&current).signing;
        let new_key = derive_half_key(
            &Bip39Half::from_entropy(&[0x7f; ENTROPY_LEN]).expect("fixed entropy encodes"),
        );
        assert_ne!(encrypted_old_private_key, *current_key.as_bytes());
        assert_eq!(
            decrypt_old_private_key(&encrypted_old_private_key, &new_key).as_bytes(),
            current_key.as_bytes(),
            "the new signing key must open the replaced one"
        );
    }

    fn assert_rotation_proof(proof: &TonCell, request: &TonCell, new_public_key: [u8; 32]) {
        let mut parser = proof.parser();
        assert_eq!(
            parser
                .read_bits(KEY_ROTATION_PROOF_TAG_BITS)
                .expect("proof tag"),
            KEY_ROTATION_PROOF_TAG
        );
        assert_eq!(parser.read_num::<u8>(8).expect("workchain"), 0);
        let _address_hash = parser.read_bits(PUBLIC_KEY_BITS).expect("address hash");
        parser.ensure_empty().expect("proof payload ends exactly");

        let new_key = VerifyingKey::from_bytes(&new_public_key).expect("new key parses");
        let mut request_parser = request.parser();
        request_parser
            .read_bits(32 + 32 + 32 + 32 + PUBLIC_KEY_BITS)
            .expect("request header and new key");
        let signature_cell = request_parser.read_next_ref().expect("proof signature ref");
        let signature = Signature::from_slice(
            &signature_cell
                .parser()
                .read_bits(SIGNATURE_BITS)
                .expect("proof signature"),
        )
        .expect("proof signature has 64 bytes");
        new_key
            .verify_strict(
                proof.cell_hash().expect("proof hashes").as_slice(),
                &signature,
            )
            .expect("new key verifies the rotation proof");
    }
}
