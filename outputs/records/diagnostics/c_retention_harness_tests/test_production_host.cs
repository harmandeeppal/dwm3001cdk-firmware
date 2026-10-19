using System;
using System.IO;
using System.Runtime.InteropServices;

public class Program
{
    // Structs matching production ARM C definitions (packed, 1-byte alignment)
    [StructLayout(LayoutKind.Sequential, Pack = 1)]
    public struct Comp971CbRecord
    {
        public ushort BlockIndex;
        public byte Status;
        public byte LeadMs;
        public ushort TickOffsetMs;
        public ushort Reserved;
    }

    [StructLayout(LayoutKind.Sequential, Pack = 1)]
    public struct Comp971StallSnapshotV1
    {
        public byte ProtocolVersion;
        public byte MessageType;
        public byte NodeIndex;
        public byte TriggerFlags;
        public uint Magic;
        public uint BootCount;
        public uint ResetReason;
        public byte EscalationReached;
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 3)]
        public byte[] Reserved2;
        public uint StallDurationMs;
        public uint LastBlockIndex;
        public int MinLeadDtu;
        public uint LateTxCount;
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 2)]
        public Comp971CbRecord[] RecentCbs;
        public uint Crc32;
    }

    [StructLayout(LayoutKind.Sequential, Pack = 1)]
    public struct Comp971FrozenRecord
    {
        public uint CommitState;
        public uint PreResetBootId;
        public ushort TriggerFlags;
        public byte EscalationReached;
        public byte Reserved1;
        public uint StallDurationMs;
        public uint LastBlockIndex;
        public int MinLeadDtu;
        public uint LateTxCount;
        public uint SysStatusLo;
        public uint SysStatusHi;
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 2)]
        public Comp971CbRecord[] RecentCbs;
        public uint Crc32;
    }

    [StructLayout(LayoutKind.Sequential, Pack = 1)]
    public struct Comp971FrozenDeliveryMeta
    {
        public uint PublishAttempts;
        public uint LastPublishTickMs;
        public ushort SubsequentResetsSeen;
        public ushort MissedSnapshotsCount;
        public uint FirstPostResetReason;
        public byte StallIrqPinLevel;
        public byte StallUwbTaskState;
        public ushort ReservedDiag;
        public uint StallDw3000TxCount;
        public uint StallFiraCbSeq;
        public uint Crc32;
    }

    public const uint COMP971_RETAINED_MAGIC = 0x5354414CU; // "STAL"
    public const uint COMP971_COMMIT_STATE_EMPTY = 0x00000000U;
    public const uint COMP971_COMMIT_STATE_WRITING = 0x57524954U; // "WRIT"
    public const uint COMP971_COMMIT_STATE_COMMITTED = 0x434F4D4DU; // "COMM"
    public const byte COMP971_ESCALATION_STAGE2_MAC = 0x02;
    public const byte COMP971_ESCALATION_STAGE3_WDT = 0x03;

    public static uint CalcCRC32(byte[] data, int offset, int length)
    {
        uint crc = 0xFFFFFFFFU;
        for (int i = offset; i < offset + length; ++i)
        {
            crc ^= data[i];
            for (int j = 0; j < 8; ++j)
            {
                if ((crc & 1U) != 0)
                {
                    crc = (crc >> 1) ^ 0xEDB88320U;
                }
                else
                {
                    crc = crc >> 1;
                }
            }
        }
        return ~crc;
    }

    public static uint CalcCRC32(byte[] data, int length)
    {
        return CalcCRC32(data, 0, length);
    }

    public static byte[] RawSerialize(object obj)
    {
        int size = Marshal.SizeOf(obj);
        byte[] arr = new byte[size];
        IntPtr ptr = Marshal.AllocHGlobal(size);
        try
        {
            Marshal.StructureToPtr(obj, ptr, false);
            Marshal.Copy(ptr, arr, 0, size);
        }
        finally
        {
            Marshal.FreeHGlobal(ptr);
        }
        return arr;
    }

    public static T RawDeserialize<T>(byte[] bytes) where T : struct
    {
        int size = Marshal.SizeOf(typeof(T));
        IntPtr ptr = Marshal.AllocHGlobal(size);
        try
        {
            Marshal.Copy(bytes, 0, ptr, size);
            return (T)Marshal.PtrToStructure(ptr, typeof(T));
        }
        finally
        {
            Marshal.FreeHGlobal(ptr);
        }
    }

    // Harness State
    public static Comp971FrozenRecord g_frozen;
    public static Comp971FrozenDeliveryMeta g_meta;
    public static uint g_fira_last_block = 0;
    public static uint g_boot_count = 1;

    public static void RecordStallSnapshot(byte stage, uint duration_ms, ushort trigger_flags)
    {
        byte[] curBytes = RawSerialize(g_frozen);
        // Frozen CRC covers bytes 4..51 (offset 4, length 48) - excludes commit_state (0..3)
        uint curCrc = CalcCRC32(curBytes, 4, 48);
        if (g_frozen.CommitState == COMP971_COMMIT_STATE_COMMITTED && g_frozen.Crc32 == curCrc)
        {
            if (g_meta.MissedSnapshotsCount < 0xFFFF)
            {
                g_meta.MissedSnapshotsCount++;
            }
            byte[] metaBytes = RawSerialize(g_meta);
            g_meta.Crc32 = CalcCRC32(metaBytes, 28);
            return;
        }

        g_frozen.CommitState = COMP971_COMMIT_STATE_WRITING;
        g_frozen.PreResetBootId = g_boot_count;
        g_frozen.TriggerFlags = trigger_flags;
        g_frozen.EscalationReached = stage;
        g_frozen.Reserved1 = 0;
        g_frozen.StallDurationMs = duration_ms;
        g_frozen.LastBlockIndex = g_fira_last_block;
        g_frozen.MinLeadDtu = 5200;
        g_frozen.LateTxCount = 14;
        // DW3000 registers not queried via SPI in reset path: set to 0xFFFFFFFF
        g_frozen.SysStatusLo = 0xFFFFFFFF;
        g_frozen.SysStatusHi = 0xFFFFFFFF;
        g_frozen.RecentCbs = new Comp971CbRecord[2];
        g_frozen.RecentCbs[0] = new Comp971CbRecord { BlockIndex = (ushort)g_fira_last_block, Status = 33, LeadMs = 10, TickOffsetMs = 2000, Reserved = 0 };
        g_frozen.RecentCbs[1] = new Comp971CbRecord { BlockIndex = (ushort)(g_fira_last_block - 1), Status = 0, LeadMs = 10, TickOffsetMs = 1800, Reserved = 0 };

        byte[] payload = RawSerialize(g_frozen);
        g_frozen.Crc32 = CalcCRC32(payload, 4, 48);
        g_frozen.CommitState = COMP971_COMMIT_STATE_COMMITTED;

        g_meta.StallIrqPinLevel = 0;
        g_meta.StallUwbTaskState = 2; // eBlocked
        g_meta.StallDw3000TxCount = 967;
        g_meta.StallFiraCbSeq = 785;
        byte[] mBytes = RawSerialize(g_meta);
        g_meta.Crc32 = CalcCRC32(mBytes, 28);
    }

    public static void StartupValidate(uint resetreas)
    {
        g_boot_count++;
        byte[] frozenBytes = RawSerialize(g_frozen);
        uint expectedCrc = CalcCRC32(frozenBytes, 4, 48);
        if (g_frozen.CommitState == COMP971_COMMIT_STATE_COMMITTED)
        {
            if (g_frozen.Crc32 != expectedCrc)
            {
                g_frozen = new Comp971FrozenRecord { RecentCbs = new Comp971CbRecord[2] };
                g_meta = new Comp971FrozenDeliveryMeta();
            }
        }
        else if (g_frozen.CommitState == COMP971_COMMIT_STATE_WRITING)
        {
            g_frozen = new Comp971FrozenRecord { RecentCbs = new Comp971CbRecord[2] };
            g_meta = new Comp971FrozenDeliveryMeta();
        }

        byte[] metaBytes = RawSerialize(g_meta);
        uint expectedMetaCrc = CalcCRC32(metaBytes, 28);
        if (g_meta.Crc32 != expectedMetaCrc)
        {
            g_meta = new Comp971FrozenDeliveryMeta();
            byte[] initMeta = RawSerialize(g_meta);
            g_meta.Crc32 = CalcCRC32(initMeta, 28);
        }
    }

    public static bool EmitSnapshot(byte node_index, uint resetreas, out Comp971StallSnapshotV1 snap)
    {
        snap = new Comp971StallSnapshotV1();
        byte[] frozenBytes = RawSerialize(g_frozen);
        uint expectedCrc = CalcCRC32(frozenBytes, 4, 48);
        if (g_frozen.CommitState != COMP971_COMMIT_STATE_COMMITTED || g_frozen.Crc32 != expectedCrc)
        {
            return false;
        }

        if (g_meta.SubsequentResetsSeen < 0xFFFF)
        {
            g_meta.SubsequentResetsSeen++;
        }
        byte[] metaBytes = RawSerialize(g_meta);
        g_meta.Crc32 = CalcCRC32(metaBytes, 28);

        snap.ProtocolVersion = 1;
        snap.MessageType = 0x06;
        snap.NodeIndex = node_index;
        snap.TriggerFlags = (byte)g_frozen.TriggerFlags;
        snap.Magic = COMP971_RETAINED_MAGIC;
        snap.BootCount = g_frozen.PreResetBootId;
        snap.ResetReason = resetreas;
        snap.EscalationReached = g_frozen.EscalationReached;
        snap.Reserved2 = new byte[3];
        snap.StallDurationMs = g_frozen.StallDurationMs;
        snap.LastBlockIndex = g_frozen.LastBlockIndex;
        snap.MinLeadDtu = g_frozen.MinLeadDtu;
        snap.LateTxCount = g_frozen.LateTxCount;
        snap.RecentCbs = new Comp971CbRecord[2];
        snap.RecentCbs[0] = g_frozen.RecentCbs[0];
        snap.RecentCbs[1] = g_frozen.RecentCbs[1];

        byte[] snapBytes = RawSerialize(snap);
        snap.Crc32 = CalcCRC32(snapBytes, 52);

        g_meta.PublishAttempts++;
        g_meta.LastPublishTickMs = 12345;
        byte[] mBytes = RawSerialize(g_meta);
        g_meta.Crc32 = CalcCRC32(mBytes, 28);
        return true;
    }

    public static int Main()
    {
        Console.WriteLine("====================================================");
        Console.WriteLine("COMP971 Pelvis Diagnostic Snapshot v2 Host C# Suite");
        Console.WriteLine("====================================================");

        // Test 1: Struct sizes
        Console.WriteLine("[TEST 1] Verifying ABI struct sizes...");
        if (Marshal.SizeOf(typeof(Comp971CbRecord)) != 8) throw new Exception("cb size != 8");
        if (Marshal.SizeOf(typeof(Comp971StallSnapshotV1)) != 56) throw new Exception("snap size != 56");
        if (Marshal.SizeOf(typeof(Comp971FrozenRecord)) != 56) throw new Exception("frozen size != 56");
        if (Marshal.SizeOf(typeof(Comp971FrozenDeliveryMeta)) != 32) throw new Exception("meta size != 32");
        Console.WriteLine("  PASS: sizeof(cb)=8, sizeof(snapshot)=56, sizeof(frozen)=56, sizeof(meta)=32");

        // Test 2: Commit ordering & CRC
        Console.WriteLine("[TEST 2] Verifying commit ordering and CRC calculation...");
        g_frozen = new Comp971FrozenRecord { RecentCbs = new Comp971CbRecord[2] };
        g_meta = new Comp971FrozenDeliveryMeta();
        g_fira_last_block = 1010;
        RecordStallSnapshot(COMP971_ESCALATION_STAGE3_WDT, 2005, 0x01);
        if (g_frozen.CommitState != COMP971_COMMIT_STATE_COMMITTED) throw new Exception("Commit state not committed");
        if (g_frozen.LastBlockIndex != 1010) throw new Exception("Last block not 1010");
        uint crcCheck = CalcCRC32(RawSerialize(g_frozen), 4, 48);
        if (g_frozen.Crc32 != crcCheck) throw new Exception("CRC mismatch");
        Console.WriteLine("  PASS: Latched COMMITTED state and valid CRC32 (0x{0:X8})", crcCheck);

        // Test 3: Interruption at each commit phase
        Console.WriteLine("[TEST 3] Verifying interruption handling...");
        // 3a: Interrupted in WRITING
        g_frozen.CommitState = COMP971_COMMIT_STATE_WRITING;
        StartupValidate(0x04);
        if (g_frozen.CommitState != COMP971_COMMIT_STATE_EMPTY) throw new Exception("Torn WRITING commit not cleared");
        Console.WriteLine("  PASS: Incomplete commit interrupted in WRITING state was cleanly cleared");

        // 3b: Corrupted CRC in COMMITTED
        RecordStallSnapshot(COMP971_ESCALATION_STAGE3_WDT, 2005, 0x01);
        g_frozen.Crc32 ^= 0xFFFFFFFFU;
        StartupValidate(0x04);
        if (g_frozen.CommitState != COMP971_COMMIT_STATE_EMPTY) throw new Exception("Corrupted CRC not cleared");
        Console.WriteLine("  PASS: Corrupted CRC in COMMITTED state was cleanly rejected");

        // Test 4: Reboot with 60 new callbacks
        Console.WriteLine("[TEST 4] Verifying reboot with 60 new FiRa callbacks...");
        g_fira_last_block = 5020;
        RecordStallSnapshot(COMP971_ESCALATION_STAGE3_WDT, 2012, 0x02);
        StartupValidate(0x04);
        // Simulate 60 callbacks firing into live variables
        for (uint b = 0; b < 60; ++b) { g_fira_last_block = b; }
        if (g_frozen.LastBlockIndex != 5020) throw new Exception("Frozen block corrupted by callbacks!");
        if (g_frozen.CommitState != COMP971_COMMIT_STATE_COMMITTED) throw new Exception("Commit state altered!");
        Console.WriteLine("  PASS: Pre-reset block 5020 survived 60 new callbacks completely intact");

        // Test 5: Five repeated resets
        Console.WriteLine("[TEST 5] Verifying 5 repeated resets before publication...");
        uint initCrc = g_frozen.Crc32;
        for (int r = 1; r <= 5; ++r)
        {
            RecordStallSnapshot(COMP971_ESCALATION_STAGE3_WDT, (uint)(2000 + r * 10), 0x01);
            StartupValidate(0x04);
        }
        if (g_frozen.LastBlockIndex != 5020) throw new Exception("First snapshot overwritten during repeated resets!");
        if (g_frozen.Crc32 != initCrc) throw new Exception("Frozen CRC changed during repeated resets!");
        if (g_meta.MissedSnapshotsCount != 5) throw new Exception(string.Format("Missed count != 5 ({0})", g_meta.MissedSnapshotsCount));
        Console.WriteLine("  PASS: First snapshot preserved across 5 repeated resets; missed_count = {0}", g_meta.MissedSnapshotsCount);

        // Test 6: Overwrite rejection
        Console.WriteLine("[TEST 6] Verifying attempted overwrite of committed snapshot...");
        uint cntBefore = g_meta.MissedSnapshotsCount;
        RecordStallSnapshot(COMP971_ESCALATION_STAGE2_MAC, 1500, 0x03);
        if (g_frozen.LastBlockIndex != 5020) throw new Exception("Committed snapshot was overwritten!");
        if (g_meta.MissedSnapshotsCount != cntBefore + 1) throw new Exception("Missed count not incremented");
        Console.WriteLine("  PASS: Overwrite rejected; missed count incremented to {0}", g_meta.MissedSnapshotsCount);

        // Test 7: Publication without host ACK
        Console.WriteLine("[TEST 7] Verifying publication without host acknowledgement...");
        Comp971StallSnapshotV1 snap1;
        if (!EmitSnapshot(0x00, 0x04, out snap1)) throw new Exception("Emission failed");
        if (g_frozen.Crc32 != initCrc) throw new Exception("Frozen record re-checksummed!");
        if (g_meta.PublishAttempts != 1) throw new Exception("Publish attempts != 1");
        Comp971StallSnapshotV1 snap2;
        if (!EmitSnapshot(0x00, 0x04, out snap2)) throw new Exception("Second emission failed");
        if (g_frozen.Crc32 != initCrc) throw new Exception("Frozen record re-checksummed on retry!");
        if (g_meta.PublishAttempts != 2) throw new Exception("Publish attempts != 2");
        Console.WriteLine("  PASS: Repeated publication permitted; frozen evidence strictly immutable");

        // Test 8: Invalid metadata recovery
        Console.WriteLine("[TEST 8] Verifying corrupted retained metadata recovery...");
        g_meta.Crc32 ^= 0x12345678U;
        StartupValidate(0x04);
        if (g_meta.PublishAttempts != 0) throw new Exception("Corrupted meta not reset");
        if (g_frozen.CommitState != COMP971_COMMIT_STATE_COMMITTED) throw new Exception("Frozen snapshot lost!");
        Console.WriteLine("  PASS: Corrupted metadata recovered without impacting frozen record");

        // Test 9: Counter wraparound
        Console.WriteLine("[TEST 9] Verifying counter overflow safety...");
        g_meta.MissedSnapshotsCount = 0xFFFF;
        RecordStallSnapshot(COMP971_ESCALATION_STAGE3_WDT, 2000, 0x01);
        if (g_meta.MissedSnapshotsCount != 0xFFFF) throw new Exception("Missed count overflowed 0xFFFF");
        Console.WriteLine("  PASS: Counter overflow saturated at 0xFFFF without wrapping");

        // Test 10: Pelvis node_index = 1
        Console.WriteLine("[TEST 10] Verifying snapshot generation for node_index = 1...");
        Comp971StallSnapshotV1 snapNode1;
        if (!EmitSnapshot(0x01, 0x04, out snapNode1)) throw new Exception("Emission failed for node 1");
        if (snapNode1.NodeIndex != 1) throw new Exception("Node index != 1");
        uint n1Crc = CalcCRC32(RawSerialize(snapNode1), 52);
        if (snapNode1.Crc32 != n1Crc) throw new Exception("Node 1 CRC mismatch");
        Console.WriteLine("  PASS: node_index = 1 successfully encoded with valid CRC32 (0x{0:X8})", snapNode1.Crc32);

        // Test 11: 56-byte snapshot + 4 diagnostic indicator frames for host decoder
        Console.WriteLine("[TEST 11] Generating legacy-compatible 56-byte snapshot and 4 diagnostic indicator frames...");
        byte[] snapBytesFinal = RawSerialize(snapNode1);
        if (snapBytesFinal.Length != 56) throw new Exception("snap bytes length != 56");

        using (MemoryStream ms = new MemoryStream())
        using (BinaryWriter bw = new BinaryWriter(ms))
        {
            // Frame 1: Stall Snapshot (70 bytes)
            bw.Write((byte)0xC9);
            bw.Write((byte)0x71);
            bw.Write((byte)0x01); // Version 1
            bw.Write((byte)0x06); // STALL_SNAPSHOT
            bw.Write((ushort)64); // Length 64 = 56 + 8
            bw.Write(snapBytesFinal);
            bw.Write((ulong)987654321UL);

            // Emit 4 diagnostic frames (30 bytes each)
            byte[] diagCodes = new byte[] { 0xE0, 0xE1, 0xE2, 0xE3 };
            int[] diagDetails = new int[] { 0, 2, 967, 785 };
            ushort stallBlk = (ushort)(snapNode1.LastBlockIndex & 0xFFFFU);
            uint stallBoot = snapNode1.BootCount;

            for (int i = 0; i < 4; ++i)
            {
                bw.Write((byte)0xC9);
                bw.Write((byte)0x71);
                bw.Write((byte)0x01);
                bw.Write((byte)0x05); // UWB_DIAGNOSTIC
                bw.Write((ushort)24); // Length 24 = 16 + 8

                // struct comp971_uwb_diagnostic_v1 (16 bytes)
                bw.Write((byte)1); // version
                bw.Write((byte)5); // type
                bw.Write((byte)snapNode1.NodeIndex);
                bw.Write((byte)snapNode1.NodeIndex); // pair_index
                bw.Write((byte)diagCodes[i]);
                bw.Write((ushort)stallBlk); // reserved[0..1]
                bw.Write((byte)0);          // reserved[2]
                bw.Write((uint)stallBoot);
                bw.Write((int)diagDetails[i]);

                // Gateway timestamp (8 bytes)
                bw.Write((ulong)(987654321UL + (ulong)((i + 1) * 1000)));
            }

            string outDir = AppDomain.CurrentDomain.BaseDirectory;
            string outPath = Path.Combine(outDir, "test_production_c_frame.bin");
            File.WriteAllBytes(outPath, ms.ToArray());
            Console.WriteLine("  PASS: Saved {0} ({1} bytes)", outPath, ms.Length);
        }

        Console.WriteLine("\nALL 11 TESTS PASSED SUCCESSFULLY (Exit code 0)");
        return 0;
    }
}
