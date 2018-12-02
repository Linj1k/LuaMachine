// Copyright 2018 - Roberto De Ioris

#include "LuaCodeCustomization.h"
#include "LuaMachineEditor.h"
#include "Runtime/Slate/Public/Widgets/Text/SMultiLineEditableText.h"
#include "Editor/PropertyEditor/Public/PropertyHandle.h"
#include "Editor/PropertyEditor/Public/DetailLayoutBuilder.h"
#include "Editor/PropertyEditor/Public/DetailCategoryBuilder.h"
#include "Editor/PropertyEditor/Public/IDetailChildrenBuilder.h"
#include "Editor/PropertyEditor/Public/DetailWidgetRow.h"
#include "Runtime/Slate/Public/Widgets/Text/STextBlock.h"
#include "Runtime/Slate/Public/Widgets/Layout/SGridPanel.h"
#include "Runtime/SlateCore/Public/Brushes/SlateColorBrush.h"
#include "Runtime/Slate/Public/Widgets/Text/SlateEditableTextLayout.h"
#include "Runtime/SlateCore/Public/Fonts/FontMeasure.h"
#include "Runtime/Slate/Public/Framework/Application/SlateApplication.h"
#include "Runtime/Slate/Public/Framework/Text/SyntaxHighlighterTextLayoutMarshaller.h"
#include "Runtime/Slate/Public/Framework/Text/SyntaxTokenizer.h"
#include "Runtime/Slate/Public/Framework/Text/TextLayout.h"
#include "Runtime/Slate/Public/Framework/Text/SlateTextRun.h"
#include "LuaMachine/Public/LuaCode.h"

class FLuaMachineSyntaxHighlighterTextLayoutMarshaller : public FSyntaxHighlighterTextLayoutMarshaller
{
public:
	struct FSyntaxTextStyle
	{
		FSyntaxTextStyle() :
			NormalTextStyle(FLuaMachineEditorModule::Get().GetStyleSet()->GetWidgetStyle<FTextBlockStyle>("SyntaxHighlight.LuaMachine.Normal")),
			CommentTextStyle(FLuaMachineEditorModule::Get().GetStyleSet()->GetWidgetStyle<FTextBlockStyle>("SyntaxHighlight.LuaMachine.Comment"))
		{}
		FTextBlockStyle NormalTextStyle;
		FTextBlockStyle CommentTextStyle;
	};

	FLuaMachineSyntaxHighlighterTextLayoutMarshaller(TSharedPtr<FSyntaxTokenizer> InTokenizer, const FSyntaxTextStyle& InSyntaxTextStyle) : FSyntaxHighlighterTextLayoutMarshaller(InTokenizer), SyntaxTextStyle(InSyntaxTextStyle)
	{

	}

	static TSharedRef< FLuaMachineSyntaxHighlighterTextLayoutMarshaller> Create()
	{
		TArray<FSyntaxTokenizer::FRule> TokenizerRules;

		TokenizerRules.Add(FSyntaxTokenizer::FRule(TEXT("--[[")));
		TokenizerRules.Add(FSyntaxTokenizer::FRule(TEXT("--]]")));
		TokenizerRules.Add(FSyntaxTokenizer::FRule(TEXT("--")));

		return MakeShareable(new FLuaMachineSyntaxHighlighterTextLayoutMarshaller(FSyntaxTokenizer::Create(TokenizerRules), FSyntaxTextStyle()));
	}
protected:
	virtual void ParseTokens(const FString& SourceString, FTextLayout& TargetTextLayout, TArray<FSyntaxTokenizer::FTokenizedLine> TokenizedLines) override
	{

		enum class EParseState : uint8
		{
			None,
			LookingForSingleLineComment,
			LookingForMultiLineComment
		};

		TArray<FTextLayout::FNewLineData> LinesToAdd;
		LinesToAdd.Reserve(TokenizedLines.Num());
		

		EParseState ParseState = EParseState::None;

		for (const FSyntaxTokenizer::FTokenizedLine& TokenizedLine : TokenizedLines)
		{
			TSharedRef<FString> ModelString = MakeShareable(new FString());
			TArray<TSharedRef<IRun>> Runs;

			if (ParseState == EParseState::LookingForSingleLineComment)
			{
				ParseState = EParseState::None;
			}

			

			for (const FSyntaxTokenizer::FToken& Token : TokenizedLine.Tokens)
			{
				const FString TokenString = SourceString.Mid(Token.Range.BeginIndex, Token.Range.Len());
				const FTextRange ModelRange(ModelString->Len(), ModelString->Len() + TokenString.Len());

				ModelString->Append(TokenString);

				FRunInfo RunInfo(TEXT("SyntaxHighlight.LuaMachine.Normal"));

				FTextBlockStyle CurrentBlockStyle = SyntaxTextStyle.NormalTextStyle;

				bool bIsWhitespace = FString(TokenString).TrimEnd().IsEmpty();
				if (!bIsWhitespace)
				{
					bool bHasMatchedSyntax = false;
					if (Token.Type == FSyntaxTokenizer::ETokenType::Syntax)
					{
						if (ParseState == EParseState::None && TokenString == TEXT("--"))
						{
							RunInfo.Name = TEXT("SyntaxHighlight.LuaMachine.Comment");
							CurrentBlockStyle = SyntaxTextStyle.CommentTextStyle;
							ParseState = EParseState::LookingForSingleLineComment;
						}
						else if (ParseState == EParseState::None && TokenString == TEXT("--[["))
						{
							RunInfo.Name = TEXT("SyntaxHighlight.LuaMachine.Comment");
							CurrentBlockStyle = SyntaxTextStyle.CommentTextStyle;
							ParseState = EParseState::LookingForMultiLineComment;
						}
						else if (ParseState == EParseState::LookingForMultiLineComment && TokenString == TEXT("--]]"))
						{
							RunInfo.Name = TEXT("SyntaxHighlight.LuaMachine.Comment");
							CurrentBlockStyle = SyntaxTextStyle.CommentTextStyle;
							ParseState = EParseState::None;
						}
					}

					if (Token.Type == FSyntaxTokenizer::ETokenType::Literal || !bHasMatchedSyntax)
					{
						if (ParseState == EParseState::LookingForSingleLineComment)
						{
							RunInfo.Name = TEXT("SyntaxHighlight.LuaMachine.Comment");
							CurrentBlockStyle = SyntaxTextStyle.CommentTextStyle;
						}
						else if (ParseState == EParseState::LookingForMultiLineComment)
						{
							RunInfo.Name = TEXT("SyntaxHighlight.LuaMachine.Comment");
							CurrentBlockStyle = SyntaxTextStyle.CommentTextStyle;
						}
					}
					TSharedRef<ISlateRun> Run = FSlateTextRun::Create(RunInfo, ModelString, CurrentBlockStyle, ModelRange);
					Runs.Add(Run);
				}
				else
				{
					RunInfo.Name = TEXT("SyntaxHighlight.LuaMachine.WhiteSpace");
					TSharedRef<ISlateRun> Run = FSlateTextRun::Create(RunInfo, ModelString, SyntaxTextStyle.NormalTextStyle, ModelRange);
					Runs.Add(Run);
				}
			}

			LinesToAdd.Emplace(MoveTemp(ModelString), MoveTemp(Runs));
		}

		TargetTextLayout.AddLines(LinesToAdd);
	}

	FSyntaxTextStyle SyntaxTextStyle;
};

class SLuaMultiLineEditableText : public SMultiLineEditableText
{
public:
	SLATE_BEGIN_ARGS(SLuaMultiLineEditableText) {}

	SLATE_ARGUMENT(TSharedPtr<SScrollBar>, HScrollBar)
	SLATE_ARGUMENT(TSharedPtr<SScrollBar>, VScrollBar)
	SLATE_ARGUMENT(TWeakObjectPtr<ULuaCode>, LuaCodeOwner);

	SLATE_END_ARGS()

		void Construct(const FArguments& InArgs)
	{
		LuaCode = InArgs._LuaCodeOwner;

		SyntaxHighlighter = FLuaMachineSyntaxHighlighterTextLayoutMarshaller::Create();

		SMultiLineEditableText::Construct(
			SMultiLineEditableText::FArguments()
			.AutoWrapText(false)
			.Margin(0.0f)
			.HScrollBar(InArgs._HScrollBar)
			.VScrollBar(InArgs._VScrollBar)
			.Text(LuaCode->Code)
			.Marshaller(SyntaxHighlighter)
			.OnTextChanged(this, &SLuaMultiLineEditableText::UpdateLuaCode)
		);

	}

	void UpdateLuaCode(const FText& InCode)
	{
		LuaCode->Code = InCode;
	}

protected:

	float GetFontHeight(const FSlateFontInfo& FontInfo) const
	{
		const TSharedRef<FSlateFontMeasure> FontMeasure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
		return FontMeasure->GetMaxCharacterHeight(FontInfo);
	}

	virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override
	{
		// assume a 80*25 vt100 terminal
		float MinimumHeight = GetFontHeight(EditableTextLayout->GetTextStyle().Font) * 25;
		FVector2D Size = SMultiLineEditableText::ComputeDesiredSize(LayoutScaleMultiplier);
		if (Size.Y < MinimumHeight)
			Size.Y = MinimumHeight;
		return Size;
	}

	virtual FReply OnKeyChar(const FGeometry& InGeometry, const FCharacterEvent& InCharacterEvent) override
	{
		const TCHAR Character = InCharacterEvent.GetCharacter();
		if (Character == TEXT('\t'))
		{
			InsertTextAtCursor(FString("  "));
			return FReply::Handled();
		}
		return SMultiLineEditableText::OnKeyChar(InGeometry, InCharacterEvent);
	}

private:
	TWeakObjectPtr<ULuaCode> LuaCode;
	TSharedPtr<FLuaMachineSyntaxHighlighterTextLayoutMarshaller> SyntaxHighlighter;
};

class SLuaEditor : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SLuaEditor) {}

	SLATE_ARGUMENT(TWeakObjectPtr<ULuaCode>, LuaCodeOwner);

	SLATE_END_ARGS()

		void Construct(const FArguments& InArgs)
	{
		LuaCode = InArgs._LuaCodeOwner;

		HorizontalScrollBar = SNew(SScrollBar).Orientation(Orient_Horizontal).Thickness(FVector2D(10.0f, 10.0f));
		VerticalScrollBar = SNew(SScrollBar).Orientation(Orient_Vertical).Thickness(FVector2D(10.0f, 10.0f));

		BackgroundColor = FSlateColorBrush(FLinearColor::Black);

		ChildSlot.VAlign(VAlign_Fill).HAlign(HAlign_Fill)[
			SNew(SBorder).BorderImage(&BackgroundColor).BorderBackgroundColor(FSlateColor(FLinearColor::White))
				[
					SNew(SGridPanel).FillColumn(0, 1.0f).FillRow(0, 2.0f)
					+ SGridPanel::Slot(0, 0)
				[
					SNew(SLuaMultiLineEditableText)
					.HScrollBar(HorizontalScrollBar)
				.VScrollBar(VerticalScrollBar)
				.LuaCodeOwner(LuaCode)
				]
			+ SGridPanel::Slot(1, 0)
				[
					VerticalScrollBar.ToSharedRef()
				]
			+ SGridPanel::Slot(0, 1)
				[
					HorizontalScrollBar.ToSharedRef()
				]
				]
		];
	}



private:
	TWeakObjectPtr<ULuaCode> LuaCode;
	TSharedPtr<SScrollBar> HorizontalScrollBar;
	TSharedPtr<SScrollBar> VerticalScrollBar;
	FSlateBrush BackgroundColor;
};

void FLuaCodeCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	TArray<TWeakObjectPtr<UObject>> Objects;
	DetailBuilder.GetObjectsBeingCustomized(Objects);

	if (Objects.Num() != 1)
	{
		return;
	}

	TWeakObjectPtr<ULuaCode> LuaCode = Cast<ULuaCode>(Objects[0].Get());

	TSharedRef<IPropertyHandle> Prop = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(ULuaCode, Code));
	DetailBuilder.HideProperty(Prop);

	IDetailCategoryBuilder& Category = DetailBuilder.EditCategory("Code");
	Category.AddCustomRow(FText::FromString("Code")).WholeRowContent()[
		SNew(SLuaEditor).LuaCodeOwner(LuaCode)
	];

}




