using UnrealBuildTool;
using System;
using System.IO;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;

public class GameplayTagIndexer : ModuleRules
{
    public GameplayTagIndexer(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core", "CoreUObject", "Engine", "GameplayTags"
        });

        GenerateTagsFromConfig();
    }

    // ── Tag parsing ────────────────────────────────────────────────

    private struct TagEntry
    {
        public string FullTag; // e.g. "Attack.Parry.L"
        public string Variable; // e.g. "Attack_Parry_L"
        public string Comment; // e.g. "Lightning Strikes"
    }

    private void GenerateTagsFromConfig()
    {
        string projectDir = Path.Combine(ModuleDirectory, "..", "..");

        var tags = new List<TagEntry>();

        // Config/DefaultGameplayTags.ini (if any)
        string defaultFile = Path.Combine(projectDir, "Config", "DefaultGameplayTags.ini");
        if (File.Exists(defaultFile))
        {
            tags.AddRange(ParseIniFile(defaultFile));
        }

        // Config/Tags/*.ini
        string configTagsDir = Path.Combine(projectDir, "Config", "Tags");
        if (Directory.Exists(configTagsDir))
        {
            foreach (string file in Directory.GetFiles(configTagsDir, "*.ini"))
            {
                tags.AddRange(ParseIniFile(file));
            }
        }

        // Deduplicate & sort
        tags = tags
            .GroupBy(tag => tag.FullTag)
            .Select(group => group.First())
            .OrderBy(tag => tag.FullTag)
            .ToList();

        // Output paths
        string publicPath = Path.Combine(ModuleDirectory, "Public");
        string privatePath = Path.Combine(ModuleDirectory, "Private");
        Directory.CreateDirectory(publicPath);
        Directory.CreateDirectory(privatePath);

        WriteIfChanged(
            Path.Combine(publicPath, "GameplayTags.generated.h"),
            BuildHeader(tags));

        WriteIfChanged(
            Path.Combine(privatePath, "GameplayTags.generated.cpp"),
            BuildSource(tags));
    }

    // ── INI parser ─────────────────────────────────────────────────

    private static readonly Regex TagRegex = new Regex(
        @"GameplayTagList=\(Tag=""([^""]+)""(?:,DevComment=""([^""]*)"")?\)",
        RegexOptions.Compiled);

    private List<TagEntry> ParseIniFile(string path)
    {
        var result = new List<TagEntry>();
        foreach (string line in File.ReadAllLines(path))
        {
            var m = TagRegex.Match(line);
            if (!m.Success) continue;

            string fullTag = m.Groups[1].Value;
            string comment = m.Groups[2].Success ? m.Groups[2].Value : "";

            result.Add(new TagEntry
            {
                FullTag = fullTag,
                Variable = fullTag.Replace('.', '_'),
                Comment = comment,
            });
        }

        return result;
    }

    // ── Code generation ────────────────────────────────────────────

    private static string BuildHeader(List<TagEntry> tags)
    {
        var sb = new StringBuilder();
        sb.AppendLine("// ──────────────────────────────────────────────────────────────");
        sb.AppendLine("//  Auto-generated from Config/Tags/*.ini — DO NOT EDIT");
        sb.AppendLine("// ──────────────────────────────────────────────────────────────");
        sb.AppendLine();
        sb.AppendLine("#pragma once");
        sb.AppendLine();
        sb.AppendLine("#include \"NativeGameplayTags.h\"");
        sb.AppendLine();
        sb.AppendLine("namespace GameplayTag");
        sb.AppendLine("{");

        foreach (var tag in tags.OrderBy(t => t.Variable))
        {
            string desc = !string.IsNullOrEmpty(tag.Comment)
                ? $"{tag.Comment} ({tag.FullTag})"
                : tag.FullTag;
            sb.AppendLine($"\t/** {desc} */");
            sb.AppendLine($"\tGAMEPLAYTAGINDEXER_API UE_DECLARE_GAMEPLAY_TAG_EXTERN({tag.Variable});");
        }

        sb.AppendLine("}");
        sb.AppendLine();
        return sb.ToString();
    }

    private static string BuildSource(List<TagEntry> tags)
    {
        var sb = new StringBuilder();
        sb.AppendLine("// ──────────────────────────────────────────────────────────────");
        sb.AppendLine("//  Auto-generated from Config/Tags/*.ini — DO NOT EDIT");
        sb.AppendLine("// ──────────────────────────────────────────────────────────────");
        sb.AppendLine();
        sb.AppendLine("#include \"GameplayTags.generated.h\"");
        sb.AppendLine();
        sb.AppendLine("namespace GameplayTag");
        sb.AppendLine("{");

        foreach (var tag in tags.OrderBy(t => t.Variable))
        {
            sb.AppendLine($"\tUE_DEFINE_GAMEPLAY_TAG({tag.Variable}, \"{tag.FullTag}\");");
        }

        sb.AppendLine("}");
        sb.AppendLine();
        return sb.ToString();
    }

    // ── File utility ───────────────────────────────────────────────

    private static void WriteIfChanged(string path, string content)
    {
        if (File.Exists(path) && File.ReadAllText(path) == content)
            return;
        File.WriteAllText(path, content);
    }
}