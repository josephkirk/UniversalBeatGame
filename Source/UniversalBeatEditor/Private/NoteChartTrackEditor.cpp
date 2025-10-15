// Copyright Epic Games, Inc. All Rights Reserved.

#include "NoteChartTrackEditor.h"
#include "MovieSceneNoteChartTrack.h"
#include "MovieSceneNoteChartSection.h"
#include "ISequencer.h"
#include "ISequencerSection.h"
#include "SequencerSectionPainter.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Styling/AppStyle.h"

#define LOCTEXT_NAMESPACE "FNoteChartTrackEditor"

/**
 * Section interface for note chart sections
 * Handles visual display and interaction in the Sequencer timeline
 */
class FNoteChartSection : public ISequencerSection
{
public:
	FNoteChartSection(UMovieSceneSection& InSectionObject)
		: SectionObject(InSectionObject)
	{
	}

	// ISequencerSection interface
	virtual UMovieSceneSection* GetSectionObject() override
	{
		return &SectionObject;
	}

	virtual FText GetSectionTitle() const override
	{
		return LOCTEXT("NoteChartSectionTitle", "Note Chart");
	}

	virtual float GetSectionHeight() const override
	{
		return 50.0f;
	}

	virtual int32 OnPaintSection(FSequencerSectionPainter& Painter) const override
	{
		return Painter.PaintSectionBackground();
	}

private:
	UMovieSceneSection& SectionObject;
};

// FNoteChartTrackEditor implementation

TSharedRef<ISequencerTrackEditor> FNoteChartTrackEditor::CreateTrackEditor(TSharedRef<ISequencer> InSequencer)
{
	return MakeShareable(new FNoteChartTrackEditor(InSequencer));
}

FNoteChartTrackEditor::FNoteChartTrackEditor(TSharedRef<ISequencer> InSequencer)
	: FMovieSceneTrackEditor(InSequencer)
{
}

bool FNoteChartTrackEditor::SupportsType(TSubclassOf<UMovieSceneTrack> Type) const
{
	return Type == UMovieSceneNoteChartTrack::StaticClass();
}

TSharedRef<ISequencerSection> FNoteChartTrackEditor::MakeSectionInterface(
	UMovieSceneSection& SectionObject,
	UMovieSceneTrack& Track,
	FGuid ObjectBinding)
{
	return MakeShareable(new FNoteChartSection(SectionObject));
}

void FNoteChartTrackEditor::BuildAddTrackMenu(FMenuBuilder& MenuBuilder)
{
	MenuBuilder.AddMenuEntry(
		LOCTEXT("AddNoteChartTrack", "Note Chart Track"),
		LOCTEXT("AddNoteChartTrackTooltip", "Adds a new track for rhythm game note charts"),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "Sequencer.Tracks.Event"),
		FUIAction(
			FExecuteAction::CreateSP(this, &FNoteChartTrackEditor::HandleAddNoteChartTrackMenuEntryExecute)
		)
	);
}

TSharedPtr<SWidget> FNoteChartTrackEditor::BuildOutlinerEditWidget(
	const FGuid& ObjectBinding,
	UMovieSceneTrack* Track,
	const FBuildEditWidgetParams& Params)
{
	return nullptr;
}

void FNoteChartTrackEditor::HandleAddNoteChartTrackMenuEntryExecute()
{
	UMovieScene* FocusedMovieScene = GetFocusedMovieScene();
	if (!FocusedMovieScene)
	{
		return;
	}

	if (FocusedMovieScene->IsReadOnly())
	{
		return;
	}

	const FScopedTransaction Transaction(LOCTEXT("AddNoteChartTrack_Transaction", "Add Note Chart Track"));
	FocusedMovieScene->Modify();

	// Create new note chart track
	UMovieSceneNoteChartTrack* NewTrack = FocusedMovieScene->AddTrack<UMovieSceneNoteChartTrack>();
	ensure(NewTrack);

	if (NewTrack)
	{
		// Create initial section
		UMovieSceneSection* NewSection = NewTrack->CreateNewSection();
		ensure(NewSection);

		if (NewSection)
		{
			NewTrack->AddSection(*NewSection);

			// Set section to infinite range by default
			NewSection->SetRange(TRange<FFrameNumber>::All());
		}
	}

	GetSequencer()->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::MovieSceneStructureItemAdded);
}

#undef LOCTEXT_NAMESPACE
