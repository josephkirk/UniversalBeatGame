// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MovieSceneTrackEditor.h"

class UMovieSceneNoteChartTrack;

/**
 * Track editor for Note Chart tracks in Sequencer
 * Handles creation and management of note chart tracks
 */
class FNoteChartTrackEditor : public FMovieSceneTrackEditor
{
public:
	/**
	 * Factory function to create an instance of this track editor
	 * @param InSequencer The sequencer instance to be used by this tool
	 * @return The new track editor instance
	 */
	static TSharedRef<ISequencerTrackEditor> CreateTrackEditor(TSharedRef<ISequencer> InSequencer);

public:
	/**
	 * Constructor
	 * @param InSequencer The sequencer instance to be used by this tool
	 */
	FNoteChartTrackEditor(TSharedRef<ISequencer> InSequencer);

	// ISequencerTrackEditor interface
	virtual bool SupportsType(TSubclassOf<UMovieSceneTrack> Type) const override;
	virtual TSharedRef<ISequencerSection> MakeSectionInterface(UMovieSceneSection& SectionObject, UMovieSceneTrack& Track, FGuid ObjectBinding) override;
	virtual void BuildAddTrackMenu(FMenuBuilder& MenuBuilder) override;
	virtual TSharedPtr<SWidget> BuildOutlinerEditWidget(const FGuid& ObjectBinding, UMovieSceneTrack* Track, const FBuildEditWidgetParams& Params) override;

private:
	/** Callback for executing the "Add Note Chart Track" menu entry */
	void HandleAddNoteChartTrackMenuEntryExecute();
};
