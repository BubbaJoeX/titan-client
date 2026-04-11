<Page
	BackgroundColor='#000000'
	BackgroundOpacity='0.70'
	Name='TerraformingSystem'
	PackSize='1,1'
	ScrollExtent='1024,768'
	Size='1024,768'
	Visible='false'
	VisualEditLock='true'
>
	<Page
		BackgroundColor='#404040'
		BackgroundOpacity='0.98'
		BackgroundTint='#00354F'
		Location='150,60'
		MinimumSize='560,620'
		Name='terraformingWindow'
		PalBgTint='back1'
		ScrollExtent='560,620'
		Size='560,620'
		UserMovable='true'
		UserResizable='false'
	>
		<Data
			Name='CodeData'
			tabContainer='content.tabContainer'
			pageShaderPaint='content.pageShaderPaint'
			pageHeightEdit='content.pageHeightEdit'
			pageRoads='content.pageRoads'
			pageAffectors='content.pageAffectors'
			pageRegions='content.pageRegions'
			tabShaderPaint='content.tabContainer.tabShaderPaint'
			tabHeightEdit='content.tabContainer.tabHeightEdit'
			tabRoads='content.tabContainer.tabRoads'
			tabAffectors='content.tabContainer.tabAffectors'
			tabRegions='content.tabContainer.tabRegions'
			comboShader='content.pageShaderPaint.comboShader'
			sliderPaintRadius='content.pageShaderPaint.sliderRadius'
			textPaintRadius='content.pageShaderPaint.textRadius'
			sliderPaintBlend='content.pageShaderPaint.sliderBlend'
			textPaintBlend='content.pageShaderPaint.textBlend'
			sliderPaintFeather='content.pageShaderPaint.sliderFeather'
			textPaintFeather='content.pageShaderPaint.textFeather'
			checkPaintPreview='content.pageShaderPaint.checkPreview'
			imageShaderPreview='content.pageShaderPaint.imagePreview'
			buttonPaintApply='content.pageShaderPaint.buttonApply'
			comboHeightMode='content.pageHeightEdit.comboMode'
			sliderHeightValue='content.pageHeightEdit.sliderHeight'
			textHeightValue='content.pageHeightEdit.textHeight'
			sliderHeightRadius='content.pageHeightEdit.sliderRadius'
			textHeightRadius='content.pageHeightEdit.textRadius'
			sliderHeightStrength='content.pageHeightEdit.sliderStrength'
			textHeightStrength='content.pageHeightEdit.textStrength'
			sliderHeightFeather='content.pageHeightEdit.sliderFeather'
			textHeightFeather='content.pageHeightEdit.textFeather'
			buttonHeightSample='content.pageHeightEdit.buttonSample'
			buttonHeightApply='content.pageHeightEdit.buttonApply'
			sliderRoadWidth='content.pageRoads.sliderWidth'
			textRoadWidth='content.pageRoads.textWidth'
			comboRoadShader='content.pageRoads.comboShader'
			checkRoadSmooth='content.pageRoads.checkSmooth'
			checkRoadFlatten='content.pageRoads.checkFlatten'
			buttonRoadStart='content.pageRoads.pageMarkers.buttonStart'
			buttonRoadEnd='content.pageRoads.pageMarkers.buttonEnd'
			buttonRoadApply='content.pageRoads.buttonApply'
			textRoadStatus='content.pageRoads.textStatus'
			textStartPos='content.pageRoads.pageMarkers.textStartPos'
			textEndPos='content.pageRoads.pageMarkers.textEndPos'
			comboAffectorType='content.pageAffectors.comboType'
			sliderAffectorRadius='content.pageAffectors.sliderRadius'
			textAffectorRadius='content.pageAffectors.textRadius'
			sliderAffectorIntensity='content.pageAffectors.sliderIntensity'
			textAffectorIntensity='content.pageAffectors.textIntensity'
			sliderAffectorFeather='content.pageAffectors.sliderFeather'
			textAffectorFeather='content.pageAffectors.textFeather'
			comboAffectorParam='content.pageAffectors.comboParam'
			sliderAffectorParam='content.pageAffectors.sliderParam'
			textAffectorParam='content.pageAffectors.textParam'
			buttonAffectorApply='content.pageAffectors.buttonApply'
			treeRegions='content.pageRegions.treeRegions'
			buttonRegionSuspend='content.pageRegions.buttonRegionSuspend'
			buttonRegionResume='content.pageRegions.buttonRegionResume'
			buttonRegionDelete='content.pageRegions.buttonDelete'
			buttonRegionDeleteAll='content.pageRegions.buttonDeleteAll'
			textRegionInfo='content.pageRegions.pageSelectedInfo.textInfo'
			textStatus='content.textStatus'
			textCityName='header.textCityName'
			buttonClose='header.buttonClose'
			buttonUndo='content.buttonUndo'
			buttonResetTerrain='content.buttonResetTerrain'
		/>

		<Page BackgroundColor='#FFFFFF' BackgroundOpacity='0.50' BackgroundTint='#00354F' Location='0,0' Name='header' PalBgTint='back1' RStyleDefault='/Styles.New.whiteOutline.rs_default' ScrollExtent='560,40' Size='560,40'>
			<Text Enabled='false' Font='bold_16' LocalText='City Terraforming Tools' Location='16,10' Name='titleText' ScrollExtent='200,22' Size='200,22' TextAlignment='Left' TextColor='#FFFFFF'>City Terraforming Tools</Text>
			<Text Enabled='false' Font='bold_12' LocalText='' Location='230,12' Name='textCityName' ScrollExtent='200,18' Size='200,18' TextAlignment='Center' TextColor='#00FFFF'></Text>
			<Button LocalText='X' Location='528,8' Name='buttonClose' ScrollExtent='24,24' Size='24,24' Style='/Styles.New.buttons.hud.style'>X</Button>
		</Page>

		<Page GetsInput='true' Location='8,48' Name='content' ScrollExtent='544,564' Size='544,564'>

			<Page BackgroundColor='#223344' BackgroundOpacity='0.60' Location='0,0' Name='tabContainer' ScrollExtent='544,32' Size='544,32'>
				<Button LocalText='Shader Paint' Location='2,2' Name='tabShaderPaint' ScrollExtent='103,28' Size='103,28' Style='/Styles.New.buttons.hud.style' TextColor='#00FFFF'>Shader Paint</Button>
				<Button LocalText='Height Edit' Location='109,2' Name='tabHeightEdit' ScrollExtent='103,28' Size='103,28' Style='/Styles.New.buttons.hud.style'>Height Edit</Button>
				<Button LocalText='Roads' Location='216,2' Name='tabRoads' ScrollExtent='103,28' Size='103,28' Style='/Styles.New.buttons.hud.style'>Roads</Button>
				<Button LocalText='Affectors' Location='323,2' Name='tabAffectors' ScrollExtent='103,28' Size='103,28' Style='/Styles.New.buttons.hud.style'>Affectors</Button>
				<Button LocalText='Regions' Location='430,2' Name='tabRegions' ScrollExtent='103,28' Size='103,28' Style='/Styles.New.buttons.hud.style'>Regions</Button>
			</Page>

			<Page BackgroundColor='#000000' BackgroundOpacity='0.40' Location='0,40' Name='pageShaderPaint' ScrollExtent='544,470' Size='544,470' Visible='true'>
				<Text Enabled='false' Font='bold_12' LocalText='Paint terrain shaders in circular regions around your position.' Location='8,8' Name='textDesc' ScrollExtent='528,20' Size='528,20' TextAlignment='Left' TextColor='#AAAAAA'>Paint terrain shaders in circular regions around your position.</Text>

				<Text Enabled='false' Font='bold_11' LocalText='Shader:' Location='8,44' Name='labelShader' ScrollExtent='80,20' Size='80,20' TextAlignment='Left' TextColor='#FFFFFF'>Shader:</Text>
				<ComboBox
					BackgroundOpacity='0.80'
					ButtonMargin='2,2,2,0'
					ButtonStyle='/Styles.New.buttons.combo_arrow.style'
					ButtonWidth='12'
					DataSource='dsShader'
					DropDownHeight='320'
					Editable='false'
					GetsInput='true'
					ListStyle='/Styles.list.combo.style_all'
					Location='96,40'
					MinimumSize='0,28'
					Name='comboShader'
					RStyleDefault='/Styles.New.combo.combo1.rs_default'
					ScrollbarStyle='/Styles.scrollbar.default.vHud3.style'
					ScrollbarWidth='12'
					ScrollExtent='320,28'
					SelectedIndex='0'
					Size='320,28'
					TextboxMargin='0,2,0,0'
					TextboxStyle='/Styles.textbox.combo.Style_new'
				/>
				<DataSource Name='dsShader'>
					<Data Name='0' Text='Loading...'/>
				</DataSource>

				<Image BackgroundColor='#222222' BackgroundOpacity='0.80' Location='424,36' Name='imagePreview' ScrollExtent='112,112' Size='112,112' SourceResource=''/>

				<Text Enabled='false' Font='bold_11' LocalText='Radius:' Location='8,84' Name='labelRadius' ScrollExtent='80,20' Size='80,20' TextAlignment='Left' TextColor='#FFFFFF'>Radius:</Text>
				<Sliderbar
					Location='96,86'
					Name='sliderRadius'
					ScrollExtent='240,20'
					Size='240,20'
					Style='/Styles.New.Slider.default.horizontal.style_test'
					LowerLimit='2'
					UpperLimit='100'
					Value='15'
				/>
				<Textbox
					BackgroundOpacity='0.85'
					BackgroundTint='#1CFFFF'
					GetsInput='true'
					Location='344,80'
					Name='textRadius'
					NumericInteger='true'
					PalBgTint='line1'
					RStyleDefault='/Styles.New.whiteOutline_palW.rs_default'
					ScrollExtent='70,32'
					Size='70,32'
					Style='/Styles.textbox.default.Style'
					Text='15'
					TextAlignment='Center'
				>15</Textbox>
				<Text Enabled='false' Font='bold_9' LocalText='m' Location='418,88' Name='labelRadiusUnit' ScrollExtent='20,16' Size='20,16' TextAlignment='Left' TextColor='#888888'>m</Text>

				<Text Enabled='false' Font='bold_11' LocalText='Blend:' Location='8,128' Name='labelBlend' ScrollExtent='80,20' Size='80,20' TextAlignment='Left' TextColor='#FFFFFF'>Blend:</Text>
				<Sliderbar
					Location='96,130'
					Name='sliderBlend'
					ScrollExtent='240,20'
					Size='240,20'
					Style='/Styles.New.Slider.default.horizontal.style_test'
					LowerLimit='0'
					UpperLimit='50'
					Value='8'
				/>
				<Textbox
					BackgroundOpacity='0.85'
					BackgroundTint='#1CFFFF'
					GetsInput='true'
					Location='344,124'
					Name='textBlend'
					NumericInteger='true'
					PalBgTint='line1'
					RStyleDefault='/Styles.New.whiteOutline_palW.rs_default'
					ScrollExtent='70,32'
					Size='70,32'
					Style='/Styles.textbox.default.Style'
					Text='8'
					TextAlignment='Center'
				>8</Textbox>
				<Text Enabled='false' Font='bold_9' LocalText='m' Location='418,132' Name='labelBlendUnit' ScrollExtent='20,16' Size='20,16' TextAlignment='Left' TextColor='#888888'>m</Text>

				<Text Enabled='false' Font='bold_11' LocalText='Feather:' Location='8,172' Name='labelFeather' ScrollExtent='80,20' Size='80,20' TextAlignment='Left' TextColor='#FFFFFF'>Feather:</Text>
				<Sliderbar
					Location='96,174'
					Name='sliderFeather'
					ScrollExtent='240,20'
					Size='240,20'
					Style='/Styles.New.Slider.default.horizontal.style_test'
					LowerLimit='0'
					UpperLimit='100'
					Value='50'
				/>
				<Textbox
					BackgroundOpacity='0.85'
					BackgroundTint='#1CFFFF'
					GetsInput='true'
					Location='344,168'
					Name='textFeather'
					NumericInteger='true'
					PalBgTint='line1'
					RStyleDefault='/Styles.New.whiteOutline_palW.rs_default'
					ScrollExtent='70,32'
					Size='70,32'
					Style='/Styles.textbox.default.Style'
					Text='50'
					TextAlignment='Center'
				>50</Textbox>
				<Text Enabled='false' Font='bold_9' LocalText='%' Location='418,176' Name='labelFeatherUnit' ScrollExtent='20,16' Size='20,16' TextAlignment='Left' TextColor='#888888'>%</Text>

				<Checkbox Location='8,216' Name='checkPreview' ScrollExtent='180,24' Size='180,24' Style='/Styles.New.checkboxes.hud.style' Text='Show Preview Circle'>Show Preview Circle</Checkbox>

				<Page BackgroundColor='#444444' BackgroundOpacity='0.30' Location='8,260' Name='pageInstructions' ScrollExtent='528,140' Size='528,140'>
					<Text Enabled='false' Font='bold_10' LocalText='Instructions:' Location='8,8' Name='labelInstr' ScrollExtent='100,16' Size='100,16' TextAlignment='Left' TextColor='#FFFF00'>Instructions:</Text>
					<Text Enabled='false' Font='bold_10' LocalText='1. Select a terrain shader from the dropdown' Location='8,28' Name='textInstr1' ScrollExtent='500,16' Size='500,16' TextAlignment='Left' TextColor='#CCCCCC'>1. Select a terrain shader from the dropdown</Text>
					<Text Enabled='false' Font='bold_10' LocalText='2. Adjust radius for area of effect' Location='8,46' Name='textInstr2' ScrollExtent='500,16' Size='500,16' TextAlignment='Left' TextColor='#CCCCCC'>2. Adjust radius for area of effect</Text>
					<Text Enabled='false' Font='bold_10' LocalText='3. Adjust blend for transition smoothness' Location='8,64' Name='textInstr3' ScrollExtent='500,16' Size='500,16' TextAlignment='Left' TextColor='#CCCCCC'>3. Adjust blend for transition smoothness</Text>
					<Text Enabled='false' Font='bold_10' LocalText='4. Adjust feather for edge softness (0=hard, 100=soft)' Location='8,82' Name='textInstr4' ScrollExtent='500,16' Size='500,16' TextAlignment='Left' TextColor='#CCCCCC'>4. Adjust feather for edge softness (0=hard, 100=soft)</Text>
					<Text Enabled='false' Font='bold_10' LocalText='5. Position yourself and click Apply' Location='8,100' Name='textInstr5' ScrollExtent='500,16' Size='500,16' TextAlignment='Left' TextColor='#CCCCCC'>5. Position yourself and click Apply</Text>
					<Text Enabled='false' Font='bold_10' LocalText='The shader will be painted centered on your location.' Location='8,118' Name='textInstr6' ScrollExtent='500,16' Size='500,16' TextAlignment='Left' TextColor='#00FF00'>The shader will be painted centered on your location.</Text>
				</Page>

				<Button LocalText='Apply Shader Paint' Location='172,416' Name='buttonApply' ScrollExtent='200,40' Size='200,40' Style='/Styles.New.buttons.hud.style'>Apply Shader Paint</Button>
			</Page>

			<Page BackgroundColor='#000000' BackgroundOpacity='0.40' Location='0,40' Name='pageHeightEdit' ScrollExtent='544,470' Size='544,470' Visible='false'>
				<Text Enabled='false' Font='bold_12' LocalText='Modify terrain height: flatten, raise, lower, or smooth.' Location='8,8' Name='textDesc' ScrollExtent='528,20' Size='528,20' TextAlignment='Left' TextColor='#AAAAAA'>Modify terrain height: flatten, raise, lower, or smooth.</Text>

				<Text Enabled='false' Font='bold_11' LocalText='Mode:' Location='8,44' Name='labelMode' ScrollExtent='80,20' Size='80,20' TextAlignment='Left' TextColor='#FFFFFF'>Mode:</Text>
				<ComboBox
					BackgroundOpacity='0.80'
					ButtonMargin='2,2,2,0'
					ButtonStyle='/Styles.New.buttons.combo_arrow.style'
					ButtonWidth='12'
					DataSource='dsHeightMode'
					DropDownHeight='150'
					Editable='false'
					GetsInput='true'
					ListStyle='/Styles.list.combo.style_all'
					Location='96,40'
					MinimumSize='0,28'
					Name='comboMode'
					RStyleDefault='/Styles.New.combo.combo1.rs_default'
					ScrollbarStyle='/Styles.scrollbar.default.vHud3.style'
					ScrollbarWidth='12'
					ScrollExtent='200,28'
					SelectedIndex='0'
					Size='200,28'
					TextboxMargin='0,2,0,0'
					TextboxStyle='/Styles.textbox.combo.Style_new'
				/>
				<DataSource Name='dsHeightMode'>
					<Data Name='0' Text='Flatten (Set Height)'/>
					<Data Name='1' Text='Raise'/>
					<Data Name='2' Text='Lower'/>
					<Data Name='3' Text='Smooth'/>
					<Data Name='4' Text='Noise'/>
				</DataSource>

				<Text Enabled='false' Font='bold_11' LocalText='Height:' Location='8,88' Name='labelHeight' ScrollExtent='80,20' Size='80,20' TextAlignment='Left' TextColor='#FFFFFF'>Height:</Text>
				<Sliderbar
					Location='96,90'
					Name='sliderHeight'
					ScrollExtent='240,20'
					Size='240,20'
					Style='/Styles.New.Slider.default.horizontal.style_test'
					LowerLimit='-200'
					UpperLimit='500'
					Value='0'
				/>
				<Textbox
					BackgroundOpacity='0.85'
					BackgroundTint='#1CFFFF'
					GetsInput='true'
					Location='344,84'
					Name='textHeight'
					NumericInteger='true'
					NumericNegative='true'
					PalBgTint='line1'
					RStyleDefault='/Styles.New.whiteOutline_palW.rs_default'
					ScrollExtent='70,32'
					Size='70,32'
					Style='/Styles.textbox.default.Style'
					Text='0'
					TextAlignment='Center'
				>0</Textbox>
				<Text Enabled='false' Font='bold_9' LocalText='m' Location='418,92' Name='labelHeightUnit' ScrollExtent='20,16' Size='20,16' TextAlignment='Left' TextColor='#888888'>m</Text>
				<Button LocalText='Sample' Location='448,84' Name='buttonSample' ScrollExtent='88,32' Size='88,32' Style='/Styles.New.buttons.hud.style'>Sample</Button>

				<Text Enabled='false' Font='bold_11' LocalText='Radius:' Location='8,132' Name='labelRadius' ScrollExtent='80,20' Size='80,20' TextAlignment='Left' TextColor='#FFFFFF'>Radius:</Text>
				<Sliderbar
					Location='96,134'
					Name='sliderRadius'
					ScrollExtent='240,20'
					Size='240,20'
					Style='/Styles.New.Slider.default.horizontal.style_test'
					LowerLimit='5'
					UpperLimit='200'
					Value='30'
				/>
				<Textbox
					BackgroundOpacity='0.85'
					BackgroundTint='#1CFFFF'
					GetsInput='true'
					Location='344,128'
					Name='textRadius'
					NumericInteger='true'
					PalBgTint='line1'
					RStyleDefault='/Styles.New.whiteOutline_palW.rs_default'
					ScrollExtent='70,32'
					Size='70,32'
					Style='/Styles.textbox.default.Style'
					Text='30'
					TextAlignment='Center'
				>30</Textbox>
				<Text Enabled='false' Font='bold_9' LocalText='m' Location='418,136' Name='labelRadiusUnit' ScrollExtent='20,16' Size='20,16' TextAlignment='Left' TextColor='#888888'>m</Text>

				<Text Enabled='false' Font='bold_11' LocalText='Strength:' Location='8,176' Name='labelStrength' ScrollExtent='80,20' Size='80,20' TextAlignment='Left' TextColor='#FFFFFF'>Strength:</Text>
				<Sliderbar
					Location='96,178'
					Name='sliderStrength'
					ScrollExtent='240,20'
					Size='240,20'
					Style='/Styles.New.Slider.default.horizontal.style_test'
					LowerLimit='1'
					UpperLimit='100'
					Value='100'
				/>
				<Textbox
					BackgroundOpacity='0.85'
					BackgroundTint='#1CFFFF'
					GetsInput='true'
					Location='344,172'
					Name='textStrength'
					NumericInteger='true'
					PalBgTint='line1'
					RStyleDefault='/Styles.New.whiteOutline_palW.rs_default'
					ScrollExtent='70,32'
					Size='70,32'
					Style='/Styles.textbox.default.Style'
					Text='100'
					TextAlignment='Center'
				>100</Textbox>
				<Text Enabled='false' Font='bold_9' LocalText='%' Location='418,180' Name='labelStrengthUnit' ScrollExtent='20,16' Size='20,16' TextAlignment='Left' TextColor='#888888'>%</Text>

				<Text Enabled='false' Font='bold_11' LocalText='Feather:' Location='8,220' Name='labelFeather' ScrollExtent='80,20' Size='80,20' TextAlignment='Left' TextColor='#FFFFFF'>Feather:</Text>
				<Sliderbar
					Location='96,222'
					Name='sliderFeather'
					ScrollExtent='240,20'
					Size='240,20'
					Style='/Styles.New.Slider.default.horizontal.style_test'
					LowerLimit='0'
					UpperLimit='100'
					Value='75'
				/>
				<Textbox
					BackgroundOpacity='0.85'
					BackgroundTint='#1CFFFF'
					GetsInput='true'
					Location='344,216'
					Name='textFeather'
					NumericInteger='true'
					PalBgTint='line1'
					RStyleDefault='/Styles.New.whiteOutline_palW.rs_default'
					ScrollExtent='70,32'
					Size='70,32'
					Style='/Styles.textbox.default.Style'
					Text='75'
					TextAlignment='Center'
				>75</Textbox>
				<Text Enabled='false' Font='bold_9' LocalText='%' Location='418,224' Name='labelFeatherUnit' ScrollExtent='20,16' Size='20,16' TextAlignment='Left' TextColor='#888888'>%</Text>

				<Page BackgroundColor='#444444' BackgroundOpacity='0.30' Location='8,270' Name='pageInfo' ScrollExtent='528,130' Size='528,130'>
					<Text Enabled='false' Font='bold_10' LocalText='Height Editing Modes:' Location='8,8' Name='labelModes' ScrollExtent='200,16' Size='200,16' TextAlignment='Left' TextColor='#FFFF00'>Height Editing Modes:</Text>
					<Text Enabled='false' Font='bold_10' LocalText='Flatten: Set terrain to exact height value' Location='8,28' Name='textMode1' ScrollExtent='500,16' Size='500,16' TextAlignment='Left' TextColor='#CCCCCC'>Flatten: Set terrain to exact height value</Text>
					<Text Enabled='false' Font='bold_10' LocalText='Raise: Increase terrain height by amount' Location='8,46' Name='textMode2' ScrollExtent='500,16' Size='500,16' TextAlignment='Left' TextColor='#CCCCCC'>Raise: Increase terrain height by amount</Text>
					<Text Enabled='false' Font='bold_10' LocalText='Lower: Decrease terrain height by amount' Location='8,64' Name='textMode3' ScrollExtent='500,16' Size='500,16' TextAlignment='Left' TextColor='#CCCCCC'>Lower: Decrease terrain height by amount</Text>
					<Text Enabled='false' Font='bold_10' LocalText='Smooth: Average heights for smoother terrain' Location='8,82' Name='textMode4' ScrollExtent='500,16' Size='500,16' TextAlignment='Left' TextColor='#CCCCCC'>Smooth: Average heights for smoother terrain</Text>
					<Text Enabled='false' Font='bold_10' LocalText='Noise: Add random variation to terrain' Location='8,100' Name='textMode5' ScrollExtent='500,16' Size='500,16' TextAlignment='Left' TextColor='#CCCCCC'>Noise: Add random variation to terrain</Text>
				</Page>

				<Button LocalText='Apply Height Edit' Location='172,416' Name='buttonApply' ScrollExtent='200,40' Size='200,40' Style='/Styles.New.buttons.hud.style'>Apply Height Edit</Button>
			</Page>

			<Page BackgroundColor='#000000' BackgroundOpacity='0.40' Location='0,40' Name='pageRoads' ScrollExtent='544,470' Size='544,470' Visible='false'>
				<Text Enabled='false' Font='bold_12' LocalText='Create roads and paths between two points.' Location='8,8' Name='textDesc' ScrollExtent='528,20' Size='528,20' TextAlignment='Left' TextColor='#AAAAAA'>Create roads and paths between two points.</Text>

				<Text Enabled='false' Font='bold_11' LocalText='Width:' Location='8,44' Name='labelWidth' ScrollExtent='80,20' Size='80,20' TextAlignment='Left' TextColor='#FFFFFF'>Width:</Text>
				<Sliderbar
					Location='96,46'
					Name='sliderWidth'
					ScrollExtent='240,20'
					Size='240,20'
					Style='/Styles.New.Slider.default.horizontal.style_test'
					LowerLimit='2'
					UpperLimit='30'
					Value='6'
				/>
				<Textbox
					BackgroundOpacity='0.85'
					BackgroundTint='#1CFFFF'
					GetsInput='true'
					Location='344,40'
					Name='textWidth'
					NumericInteger='true'
					PalBgTint='line1'
					RStyleDefault='/Styles.New.whiteOutline_palW.rs_default'
					ScrollExtent='70,32'
					Size='70,32'
					Style='/Styles.textbox.default.Style'
					Text='6'
					TextAlignment='Center'
				>6</Textbox>
				<Text Enabled='false' Font='bold_9' LocalText='m' Location='418,48' Name='labelWidthUnit' ScrollExtent='20,16' Size='20,16' TextAlignment='Left' TextColor='#888888'>m</Text>

				<Text Enabled='false' Font='bold_11' LocalText='Surface:' Location='8,88' Name='labelShader' ScrollExtent='80,20' Size='80,20' TextAlignment='Left' TextColor='#FFFFFF'>Surface:</Text>
				<ComboBox
					BackgroundOpacity='0.80'
					ButtonMargin='2,2,2,0'
					ButtonStyle='/Styles.New.buttons.combo_arrow.style'
					ButtonWidth='12'
					DataSource='dsRoadShader'
					DropDownHeight='200'
					Editable='false'
					GetsInput='true'
					ListStyle='/Styles.list.combo.style_all'
					Location='96,84'
					MinimumSize='0,28'
					Name='comboShader'
					RStyleDefault='/Styles.New.combo.combo1.rs_default'
					ScrollbarStyle='/Styles.scrollbar.default.vHud3.style'
					ScrollbarWidth='12'
					ScrollExtent='320,28'
					SelectedIndex='0'
					Size='320,28'
					TextboxMargin='0,2,0,0'
					TextboxStyle='/Styles.textbox.combo.Style_new'
				/>
				<DataSource Name='dsRoadShader'>
					<Data Name='0' Text='Loading...'/>
				</DataSource>

				<Page BackgroundColor='#333333' BackgroundOpacity='0.50' Location='8,130' Name='pageMarkers' ScrollExtent='528,120' Size='528,120'>
					<Text Enabled='false' Font='bold_11' LocalText='Road Markers:' Location='8,8' Name='labelMarkers' ScrollExtent='120,20' Size='120,20' TextAlignment='Left' TextColor='#FFFF00'>Road Markers:</Text>

					<Text Enabled='false' Font='bold_10' LocalText='Start Point:' Location='8,36' Name='labelStart' ScrollExtent='80,18' Size='80,18' TextAlignment='Left' TextColor='#FFFFFF'>Start Point:</Text>
					<Text Enabled='false' Font='bold_10' LocalText='Not Set' Location='96,36' Name='textStartPos' ScrollExtent='200,18' Size='200,18' TextAlignment='Left' TextColor='#FF6666'>Not Set</Text>
					<Button LocalText='Set Start Here' Location='320,30' Name='buttonStart' ScrollExtent='120,28' Size='120,28' Style='/Styles.New.buttons.hud.style'>Set Start Here</Button>

					<Text Enabled='false' Font='bold_10' LocalText='End Point:' Location='8,72' Name='labelEnd' ScrollExtent='80,18' Size='80,18' TextAlignment='Left' TextColor='#FFFFFF'>End Point:</Text>
					<Text Enabled='false' Font='bold_10' LocalText='Not Set' Location='96,72' Name='textEndPos' ScrollExtent='200,18' Size='200,18' TextAlignment='Left' TextColor='#FF6666'>Not Set</Text>
					<Button LocalText='Set End Here' Location='320,66' Name='buttonEnd' ScrollExtent='120,28' Size='120,28' Style='/Styles.New.buttons.hud.style'>Set End Here</Button>
				</Page>

				<Checkbox Location='8,264' Name='checkSmooth' ScrollExtent='200,24' Size='200,24' Style='/Styles.New.checkboxes.hud.style' Text='Smooth Road Edges' Checked='true'>Smooth Road Edges</Checkbox>
				<Checkbox Location='8,292' Name='checkFlatten' ScrollExtent='200,24' Size='200,24' Style='/Styles.New.checkboxes.hud.style' Text='Flatten Terrain Under Road'>Flatten Terrain Under Road</Checkbox>

				<Page BackgroundColor='#444444' BackgroundOpacity='0.30' Location='8,330' Name='pageInstr' ScrollExtent='528,70' Size='528,70'>
					<Text Enabled='false' Font='bold_10' LocalText='How to create a road:' Location='8,8' Name='labelHow' ScrollExtent='200,16' Size='200,16' TextAlignment='Left' TextColor='#FFFF00'>How to create a road:</Text>
					<Text Enabled='false' Font='bold_10' LocalText='1. Walk to the starting location and click &quot;Set Start Here&quot;' Location='8,28' Name='textHow1' ScrollExtent='500,16' Size='500,16' TextAlignment='Left' TextColor='#CCCCCC'>1. Walk to the starting location and click "Set Start Here"</Text>
					<Text Enabled='false' Font='bold_10' LocalText='2. Walk to the ending location and click &quot;Set End Here&quot;' Location='8,46' Name='textHow2' ScrollExtent='500,16' Size='500,16' TextAlignment='Left' TextColor='#CCCCCC'>2. Walk to the ending location and click "Set End Here"</Text>
				</Page>

				<Text Enabled='false' Font='bold_11' LocalText='' Location='8,408' Name='textStatus' ScrollExtent='528,24' Size='528,24' TextAlignment='Center' TextColor='#00FF00'></Text>

				<Button LocalText='Create Road' Location='172,436' Name='buttonApply' ScrollExtent='200,32' Size='200,32' Style='/Styles.New.buttons.hud.style' Enabled='false'>Create Road</Button>
			</Page>

			<Page BackgroundColor='#000000' BackgroundOpacity='0.40' Location='0,40' Name='pageAffectors' ScrollExtent='544,470' Size='544,470' Visible='false'>
				<Text Enabled='false' Font='bold_12' LocalText='Apply terrain affectors for special effects.' Location='8,8' Name='textDesc' ScrollExtent='528,20' Size='528,20' TextAlignment='Left' TextColor='#AAAAAA'>Apply terrain affectors for special effects.</Text>

				<Text Enabled='false' Font='bold_11' LocalText='Type:' Location='8,44' Name='labelType' ScrollExtent='80,20' Size='80,20' TextAlignment='Left' TextColor='#FFFFFF'>Type:</Text>
				<ComboBox
					BackgroundOpacity='0.80'
					ButtonMargin='2,2,2,0'
					ButtonStyle='/Styles.New.buttons.combo_arrow.style'
					ButtonWidth='12'
					DataSource='dsAffectorType'
					DropDownHeight='200'
					Editable='false'
					GetsInput='true'
					ListStyle='/Styles.list.combo.style_all'
					Location='96,40'
					MinimumSize='0,28'
					Name='comboType'
					RStyleDefault='/Styles.New.combo.combo1.rs_default'
					ScrollbarStyle='/Styles.scrollbar.default.vHud3.style'
					ScrollbarWidth='12'
					ScrollExtent='280,28'
					SelectedIndex='0'
					Size='280,28'
					TextboxMargin='0,2,0,0'
					TextboxStyle='/Styles.textbox.combo.Style_new'
				/>
				<DataSource Name='dsAffectorType'>
					<Data Name='0' Text='Height Constant'/>
					<Data Name='1' Text='Height Fractal'/>
					<Data Name='2' Text='Color Constant'/>
					<Data Name='3' Text='Color Ramp Height'/>
					<Data Name='4' Text='Flora Density'/>
					<Data Name='5' Text='Exclude (No Flora)'/>
					<Data Name='6' Text='Water Plane'/>
					<Data Name='7' Text='Passable'/>
					<Data Name='8' Text='Non-Passable'/>
				</DataSource>

				<Text Enabled='false' Font='bold_11' LocalText='Radius:' Location='8,88' Name='labelRadius' ScrollExtent='80,20' Size='80,20' TextAlignment='Left' TextColor='#FFFFFF'>Radius:</Text>
				<Sliderbar
					Location='96,90'
					Name='sliderRadius'
					ScrollExtent='240,20'
					Size='240,20'
					Style='/Styles.New.Slider.default.horizontal.style_test'
					LowerLimit='5'
					UpperLimit='200'
					Value='25'
				/>
				<Textbox
					BackgroundOpacity='0.85'
					BackgroundTint='#1CFFFF'
					GetsInput='true'
					Location='344,84'
					Name='textRadius'
					NumericInteger='true'
					PalBgTint='line1'
					RStyleDefault='/Styles.New.whiteOutline_palW.rs_default'
					ScrollExtent='70,32'
					Size='70,32'
					Style='/Styles.textbox.default.Style'
					Text='25'
					TextAlignment='Center'
				>25</Textbox>
				<Text Enabled='false' Font='bold_9' LocalText='m' Location='418,92' Name='labelRadiusUnit' ScrollExtent='20,16' Size='20,16' TextAlignment='Left' TextColor='#888888'>m</Text>

				<Text Enabled='false' Font='bold_11' LocalText='Intensity:' Location='8,132' Name='labelIntensity' ScrollExtent='80,20' Size='80,20' TextAlignment='Left' TextColor='#FFFFFF'>Intensity:</Text>
				<Sliderbar
					Location='96,134'
					Name='sliderIntensity'
					ScrollExtent='240,20'
					Size='240,20'
					Style='/Styles.New.Slider.default.horizontal.style_test'
					LowerLimit='0'
					UpperLimit='100'
					Value='100'
				/>
				<Textbox
					BackgroundOpacity='0.85'
					BackgroundTint='#1CFFFF'
					GetsInput='true'
					Location='344,128'
					Name='textIntensity'
					NumericInteger='true'
					PalBgTint='line1'
					RStyleDefault='/Styles.New.whiteOutline_palW.rs_default'
					ScrollExtent='70,32'
					Size='70,32'
					Style='/Styles.textbox.default.Style'
					Text='100'
					TextAlignment='Center'
				>100</Textbox>
				<Text Enabled='false' Font='bold_9' LocalText='%' Location='418,136' Name='labelIntensityUnit' ScrollExtent='20,16' Size='20,16' TextAlignment='Left' TextColor='#888888'>%</Text>

				<Text Enabled='false' Font='bold_11' LocalText='Feather:' Location='8,176' Name='labelFeather' ScrollExtent='80,20' Size='80,20' TextAlignment='Left' TextColor='#FFFFFF'>Feather:</Text>
				<Sliderbar
					Location='96,178'
					Name='sliderFeather'
					ScrollExtent='240,20'
					Size='240,20'
					Style='/Styles.New.Slider.default.horizontal.style_test'
					LowerLimit='0'
					UpperLimit='100'
					Value='50'
				/>
				<Textbox
					BackgroundOpacity='0.85'
					BackgroundTint='#1CFFFF'
					GetsInput='true'
					Location='344,172'
					Name='textFeather'
					NumericInteger='true'
					PalBgTint='line1'
					RStyleDefault='/Styles.New.whiteOutline_palW.rs_default'
					ScrollExtent='70,32'
					Size='70,32'
					Style='/Styles.textbox.default.Style'
					Text='50'
					TextAlignment='Center'
				>50</Textbox>
				<Text Enabled='false' Font='bold_9' LocalText='%' Location='418,180' Name='labelFeatherUnit' ScrollExtent='20,16' Size='20,16' TextAlignment='Left' TextColor='#888888'>%</Text>

				<Text Enabled='false' Font='bold_11' LocalText='Parameter:' Location='8,220' Name='labelParam' ScrollExtent='80,20' Size='80,20' TextAlignment='Left' TextColor='#FFFFFF'>Parameter:</Text>
				<ComboBox
					BackgroundOpacity='0.80'
					ButtonMargin='2,2,2,0'
					ButtonStyle='/Styles.New.buttons.combo_arrow.style'
					ButtonWidth='12'
					DataSource='dsParam'
					DropDownHeight='100'
					Editable='false'
					GetsInput='true'
					ListStyle='/Styles.list.combo.style_all'
					Location='96,216'
					MinimumSize='0,28'
					Name='comboParam'
					RStyleDefault='/Styles.New.combo.combo1.rs_default'
					ScrollbarStyle='/Styles.scrollbar.default.vHud3.style'
					ScrollbarWidth='12'
					ScrollExtent='160,28'
					SelectedIndex='0'
					Size='160,28'
					TextboxMargin='0,2,0,0'
					TextboxStyle='/Styles.textbox.combo.Style_new'
				/>
				<DataSource Name='dsParam'>
					<Data Name='0' Text='Default'/>
				</DataSource>

				<Sliderbar
					Location='264,218'
					Name='sliderParam'
					ScrollExtent='140,20'
					Size='140,20'
					Style='/Styles.New.Slider.default.horizontal.style_test'
					LowerLimit='0'
					UpperLimit='100'
					Value='50'
				/>
				<Textbox
					BackgroundOpacity='0.85'
					BackgroundTint='#1CFFFF'
					GetsInput='true'
					Location='412,212'
					Name='textParam'
					NumericInteger='true'
					PalBgTint='line1'
					RStyleDefault='/Styles.New.whiteOutline_palW.rs_default'
					ScrollExtent='60,32'
					Size='60,32'
					Style='/Styles.textbox.default.Style'
					Text='50'
					TextAlignment='Center'
				>50</Textbox>

				<Page BackgroundColor='#444444' BackgroundOpacity='0.30' Location='8,270' Name='pageAffectorInfo' ScrollExtent='528,130' Size='528,130'>
					<Text Enabled='false' Font='bold_10' LocalText='Affector Types:' Location='8,8' Name='labelTypes' ScrollExtent='200,16' Size='200,16' TextAlignment='Left' TextColor='#FFFF00'>Affector Types:</Text>
					<Text Enabled='false' Font='bold_10' LocalText='Height Constant: Set area to specific height' Location='8,28' Name='textType1' ScrollExtent='500,16' Size='500,16' TextAlignment='Left' TextColor='#CCCCCC'>Height Constant: Set area to specific height</Text>
					<Text Enabled='false' Font='bold_10' LocalText='Height Fractal: Add natural variation' Location='8,46' Name='textType2' ScrollExtent='500,16' Size='500,16' TextAlignment='Left' TextColor='#CCCCCC'>Height Fractal: Add natural variation</Text>
					<Text Enabled='false' Font='bold_10' LocalText='Color: Modify terrain coloring' Location='8,64' Name='textType3' ScrollExtent='500,16' Size='500,16' TextAlignment='Left' TextColor='#CCCCCC'>Color: Modify terrain coloring</Text>
					<Text Enabled='false' Font='bold_10' LocalText='Flora Density: Control vegetation amount' Location='8,82' Name='textType4' ScrollExtent='500,16' Size='500,16' TextAlignment='Left' TextColor='#CCCCCC'>Flora Density: Control vegetation amount</Text>
					<Text Enabled='false' Font='bold_10' LocalText='Passable/Non-Passable: Control movement' Location='8,100' Name='textType5' ScrollExtent='500,16' Size='500,16' TextAlignment='Left' TextColor='#CCCCCC'>Passable/Non-Passable: Control movement</Text>
				</Page>

				<Button LocalText='Apply Affector' Location='172,416' Name='buttonApply' ScrollExtent='200,40' Size='200,40' Style='/Styles.New.buttons.hud.style'>Apply Affector</Button>
			</Page>

			<Page BackgroundColor='#000000' BackgroundOpacity='0.40' Location='0,40' Name='pageRegions' ScrollExtent='544,470' Size='544,470' Visible='false'>
				<Text Enabled='false' Font='bold_12' LocalText='Manage existing terrain modifications in this city.' Location='8,8' Name='textDesc' ScrollExtent='528,20' Size='528,20' TextAlignment='Left' TextColor='#AAAAAA'>Manage existing terrain modifications in this city.</Text>

				<Text Enabled='false' Font='bold_11' LocalText='Terrain rules (by type):' Location='8,36' Name='labelRegions' ScrollExtent='200,20' Size='200,20' TextAlignment='Left' TextColor='#FFFFFF'>Terrain rules (by type):</Text>

				<TreeView
					BackgroundColor='#000000'
					BackgroundOpacity='0.60'
					DataSourceContainer='regionsTreeData'
					GetsInput='true'
					Location='8,60'
					Name='treeRegions'
					RStyleDefault='/Styles.New.whiteOutline.rs_default'
					ScrollExtent='528,260'
					SelectedRow='0'
					SelectedRowCount='0'
					SelectionAllowedMultiRow='false'
					ShowIcon='false'
					Size='528,260'
					Style='/Styles.tree.style'
				/>
				<DataSourceContainer Name='regionsTreeData' />

				<Page BackgroundColor='#333333' BackgroundOpacity='0.40' Location='8,328' Name='pageSelectedInfo' ScrollExtent='528,80' Size='528,80'>
					<Text Enabled='false' Font='bold_11' LocalText='Selected Region Info:' Location='8,8' Name='labelInfo' ScrollExtent='200,20' Size='200,20' TextAlignment='Left' TextColor='#FFFF00'>Selected Region Info:</Text>
					<Text Enabled='false' Font='bold_10' LocalText='Select a rule in the tree above.' Location='8,32' Name='textInfo' ScrollExtent='512,40' Size='512,40' TextAlignment='Left' TextColor='#CCCCCC' TextWrap='true'>Select a rule in the tree above.</Text>
				</Page>

				<Button LocalText='Suspend' Location='8,420' Name='buttonRegionSuspend' ScrollExtent='88,36' Size='88,36' Style='/Styles.New.buttons.hud.style' Enabled='false'>Suspend</Button>
				<Button LocalText='Resume' Location='100,420' Name='buttonRegionResume' ScrollExtent='88,36' Size='88,36' Style='/Styles.New.buttons.hud.style' Enabled='false'>Resume</Button>
				<Button LocalText='Delete Rule' Location='192,420' Name='buttonDelete' ScrollExtent='120,36' Size='120,36' Style='/Styles.New.buttons.hud.style' Enabled='false'>Delete Rule</Button>
				<Button LocalText='Delete All Regions' Location='318,420' Name='buttonDeleteAll' ScrollExtent='218,36' Size='218,36' Style='/Styles.New.buttons.hud.style'>Delete All Regions</Button>
			</Page>

			<Text Enabled='false' Font='bold_11' LocalText='' Location='0,520' Name='textStatus' ScrollExtent='544,24' Size='544,24' TextAlignment='Center' TextColor='#00FF00'></Text>

			<Button LocalText='Undo Last' Location='8,520' Name='buttonUndo' ScrollExtent='100,32' Size='100,32' Style='/Styles.New.buttons.hud.style'>Undo Last</Button>

			<Button BackgroundColor='#880000' BackgroundOpacity='1.00' LocalText='RESET TERRAIN' Location='412,520' Name='buttonResetTerrain' PalBgTint='/Palette.ui_warning' ScrollExtent='124,32' Size='124,32' Style='/Styles.New.buttons.hud.style' TextColor='#FF6666'>RESET TERRAIN</Button>
		</Page>
	</Page>
</Page>

